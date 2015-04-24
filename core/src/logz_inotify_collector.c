#include "logzilla_defs.h"
#include "logz_inotifier.h"
#include "timespec.h"
#include "hash.h"
#include "xalloc.h"
#include "logzilla.h"
#include "stat-time.h"
#include "safe-read.h"

#include <stdlib.h>
#include <assert.h>
#include <libgen.h>

extern enum LOG_FILE_HEADER header;

static void
recheck (struct logz_file_def *filedef, bool blocking)
{
    /* open/fstat the file and announce if dev/ino have changed */
    struct stat new_stats;
    bool ok = true;

    bool was_worth_reading = filedef->proc_worth;
    int prev_errnum = filedef->errnum;
    bool new_file;
    int fd = open (filedef->name, O_RDONLY | O_NONBLOCK);

    /* If the open fails because the file doesn't exist,
       then mark the file as not tailable.  */
    filedef->proc_worth = !(fd == -1);

    if (! lstat (filedef->name, &new_stats) && S_ISLNK (new_stats.st_mode)) {
        ok = false;
        filedef->errnum = -1;
        filedef->ignore = true;
        LOG_ERROR("%s has been replaced with a symbolic link. "
                  "giving up on this name", filedef->name);
    } else if (fd == -1 || fstat (fd, &new_stats) < 0) {
        ok = false;
        filedef->errnum = errno;
        if (!filedef->proc_worth) {
            if (was_worth_reading) {

                LOG_ERROR("%s has become inaccessible", filedef->name);
            } else {
                /* say changes. it still can't be read */
            }
        } else if (prev_errnum != errno) {
            LOG_ERROR("file in error:%s", filedef->name);
        }
    } else if (!CHECK_WORTH (new_stats.st_mode)) {
        ok = false;
        filedef->errnum = -1;
        LOG_ERROR("%s is not a regular file anymore. giving up on this name", filedef->name);
        filedef->ignore = true;
    } else {
        filedef->errnum = 0;
    }

    new_file = false;
    if (!ok) {
        logz_close_fd (fd, filedef->name);
        logz_close_fd (filedef->fd, filedef->name);
        filedef->fd = -1;
    } else if (prev_errnum && prev_errnum != ENOENT) {
        new_file = true;
        assert (filedef->fd == -1);
        LOG_ERROR("%s isn't accessible anymore", filedef->name);
    } else if (filedef->ino != new_stats.st_ino || filedef->dev != new_stats.st_dev) {
        new_file = true;
        if (filedef->fd == -1) {
            LOG_ERROR("%s has appeared; will follow end of new file",
                      filedef->name);
        } else {
            /* Close the old one.  */
            logz_close_fd (filedef->fd, filedef->name);

            /* File has been replaced (e.g., via log rotation) --
               tail the new one.  */
            LOG_ERROR("%s has been replaced; will follow end of new file", filedef->name);
        }
    } else {
        if (filedef->fd == -1) {
            /* This happens when one iteration finds the file missing,
               then the preceding <dev,inode> pair is reused as the
               file is recreated.  */
            new_file = true;
        } else {
            logz_close_fd (fd, filedef->name);
        }
    }

    if (new_file) {
        /* Start at the beginning of the file.  */

        logz_remember_fd (filedef, fd, 0, &new_stats, blocking);
        xlseek (fd, 0, SEEK_SET, filedef->name);
    }
}


static size_t
wd_hasher (const void *entry, size_t tabsize)
{
    const struct logz_file_def *spec = entry;
    return spec->wd % tabsize;
}

static bool
wd_comparator (const void *e1, const void *e2)
{
    const struct logz_file_def *spec1 = e1;
    const struct logz_file_def *spec2 = e2;
    return spec1->wd == spec2->wd;
}

static void
verify_logz_file (
    struct logz_file_def *filedef,
    int wd,
    int *prev_wd,
    FILE *target_stream) {

    struct stat stats;
    char const *name = filedef->name;

    if (filedef->fd == -1)
        return;

    if (fstat (filedef->fd, &stats) != 0) {
        filedef->errnum = errno;
        logz_close_fd (filedef->fd, name);
        filedef->fd = -1;
        return;
    }

    if (S_ISREG (filedef->mode) && stats.st_size < filedef->size) {
        LOG_ERROR("%s: file truncated", name);
        *prev_wd = wd;
        xlseek (filedef->fd, stats.st_size, SEEK_SET, name);
        filedef->size = stats.st_size;
    } else if (S_ISREG (filedef->mode)
               && stats.st_size == filedef->size
               && timespec_cmp (filedef->mtime, get_stat_mtime (&stats)) == 0)
        return;

    if (wd != *prev_wd) {
        if (header == TOGGLE) {
            //  write_header (name);
        }
        *prev_wd = wd;
    }

    uintmax_t bytes_read = dump_remainder (name, filedef->fd, COPY_TO_EOF, target_stream);
    filedef->size += bytes_read;

    if (fflush (target_stream) != 0)
        LOG_ERROR("write error");
}

bool
logz_recursive_inotify (
    int wd,
    struct logz_file_def *f,
    size_t n_files,
    FILE *target_stream) {

    unsigned int max_realloc = 3;

    Hash_table *wd_to_name;

    bool found_unwatchable_dir = false;
    bool no_inotify_resources = false;

    int prev_wd;
    size_t evlen = 0;
    char *evbuf;
    size_t evbuf_off = 0;
    size_t len = 0;

    wd_to_name = hash_initialize (n_files, NULL, wd_hasher, wd_comparator, NULL);
    if (! wd_to_name)
        xalloc_die();

    size_t i;
    for (i = 0; i < n_files; i++) {
        if (!f[i].ignore) {
            size_t fnlen = strlen (f[i].name);
            if (evlen < fnlen)
                evlen = fnlen;

            f[i].wd = -1;

            size_t dirlen = strlen(dirname (f[i].name));
            char prev = f[i].name[dirlen];
            f[i].basename_start = basename (f[i].name) - f[i].name;

            f[i].name[dirlen] = '\0';

            /* It's fine to add the same directory more than once.
               In that case the same watch descriptor is returned.  */
            f[i].parent_wd = inotify_add_watch (wd, dirlen ? f[i].name : ".", (IN_CREATE | IN_MODIFY | IN_MOVED_TO | IN_ATTRIB));

            f[i].name[dirlen] = prev;

            if (f[i].parent_wd < 0) {
                if (errno != ENOSPC)
                    LOG_ERROR("cannot watch parent directory of %s", f[i].name);
                else
                    LOG_ERROR("%s","inotify resources exhausted");
                found_unwatchable_dir = true;
                break;
            }


            f[i].wd = inotify_add_watch (wd, f[i].name, inotify_file_watch_mask);

            if (f[i].wd < 0) {
                if (errno == ENOSPC) {
                    no_inotify_resources = true;
                    LOG_ERROR("%s", "inotify resources exhausted");
                } else if (errno != f[i].errnum)
                    LOG_ERROR("cannot watch %s", f[i].name);
                continue;
            }

            if (hash_insert (wd_to_name, &(f[i])) == NULL)
                xalloc_die ();

        }
    }


    if (no_inotify_resources || found_unwatchable_dir) {
        /* FIXME: release hash and inotify resources allocated above.  */
        errno = 0;
        abort();
    }

    prev_wd = f[n_files - 1].wd;

    /* Check files again.  New data can be available since last time we checked  and before they are watched by inotify.  */
    for (i = 0; i < n_files; i++) {
        if (!f[i].ignore)
            verify_logz_file (&f[i], f[i].wd, &prev_wd, target_stream);
    }

    evlen += sizeof (struct inotify_event) + 1;
    evbuf = xmalloc (evlen);

    /* Wait for inotify events and handle them */
    while (1) {
        struct logz_file_def *fspec;
        struct inotify_event *ev;
        void *void_ev;

        /* When following by name without --retry, and the last file has
           been unlinked or renamed-away, diagnose it and return.  */
        if (hash_get_n_entries (wd_to_name) == 0) {
            LOG_ERROR("%s", "no files remaining");
            return false;
        }


        if (len <= evbuf_off) {
            len = safe_read (wd, evbuf, evlen);
            evbuf_off = 0;

            if ((len == 0 || (len == SAFE_READ_ERROR && errno == EINVAL)) && max_realloc--) {
                len = 0;
                evlen *= 2;
                evbuf = xrealloc (evbuf, evlen);
                continue;
            }

            if (len == 0 || len == SAFE_READ_ERROR)
                LOG_ERROR("%s", "error reading inotify event");
        }

        void_ev = evbuf + evbuf_off;
        ev = void_ev;
        evbuf_off += sizeof (*ev) + ev->len;

        if (ev->len) {
            size_t j;
            for (j = 0; j < n_files; j++) {
                // now this in O(n^2) be careful with file count. use hashes instead? fix..
                if (f[j].parent_wd == ev->wd
                    && strcmp (ev->name, f[j].name + f[j].basename_start))
                    break;
            }

            /* not a watched file.  */
            if (j == n_files)
                continue;

            int new_wd = inotify_add_watch (wd, f[j].name, inotify_file_watch_mask);
            if (new_wd < 0) {
                LOG_ERROR("cannot watch %s", f[j].name);
                continue;
            }

            fspec = &(f[j]);

            /* Remove 'fspec' and re-add it using 'new_fd' as its key */
            hash_delete (wd_to_name, fspec);
            fspec->wd = new_wd;

            /* If the file was moved then inotify will use the source file wd for the destination file.  Make sure the key is not present in the table.  */
            struct logz_file_def *prev = hash_delete (wd_to_name, fspec);
            if (prev && prev != fspec) {
                recheck (prev, false);
                prev->wd = -1;
                logz_close_fd (prev->fd, prev->name);
            }

            if (hash_insert (wd_to_name, fspec) == NULL)
                xalloc_die ();

            recheck (fspec, false);
        } else {
            struct logz_file_def key;
            key.wd = ev->wd;
            fspec = hash_lookup (wd_to_name, &key);
        }

        if (! fspec)
            continue;

        if (ev->mask & (IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF)) {
            /* For IN_DELETE_SELF, we always want to remove the watch.            However, for IN_MOVE_SELF (the file we're watching has
               been clobbered via a rename), we continue the watch */
            if ((ev->mask & IN_DELETE_SELF)) {
                inotify_rm_watch (wd, fspec->wd);
                hash_delete (wd_to_name, fspec);
            }
            recheck (fspec, false);

            continue;
        }
        verify_logz_file(fspec, ev->wd, &prev_wd, target_stream);
    }
}
