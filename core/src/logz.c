#include <assert.h>

#include "logzilla.h"
#include <string.h>
#include "logzilla_defs.h"

#include "fcntl--.h" //open-safer
#include "safe-read.h"
#include "stat-time.h"
#include "xalloc.h"

#include "logz_inotifier.h"

enum LOG_FILE_HEADER header;

void
logz_close_fd (int fd, const char *filename) {
    if (-1 != fd && close (fd))
        LOG_ERROR("failed to close file:%s (%d)", filename, fd);
}

void
logz_remember_fd (
    struct logz_file_def *file,
    int fd,
    off_t size,
    struct stat const *filestat,
    int blocking) {

    file->fd = fd;
    file->size = size;
    file->mtime = get_stat_mtime (filestat);
    file->dev = filestat->st_dev;
    file->ino = filestat->st_ino;
    file->mode = filestat->st_mode;
    file->blocking = blocking;
    file->passive_threshold = 0;
    file->ignore = false;
}

off_t
xlseek (int fd, off_t offset, int limit, const char *filename) {
    off_t new_offset = lseek (fd, offset, limit);

    if (0 <= new_offset)
        return new_offset;

    switch (limit) {
    case SEEK_SET:
        LOG_ERROR("cannot seek offset:%zu on file:%s", offset, filename);
        break;
    case SEEK_CUR:
        LOG_ERROR("cannot seek relative offset:%zu on file:%s", offset, filename);
        break;
    case SEEK_END:
        LOG_ERROR("cannot seek end-relative offset:%zu on file:%s", offset, filename);
        break;
    default:
        abort ();
    }

    exit(EXIT_FAILURE); //TODO..no-exit
}

void
write_out_stream (const char *buf, size_t num, FILE *outfile) {
    if ((0 < num) && (num > fwrite(buf, 1, num, outfile))) {
        LOG_ERROR("%s", "failed write attempt on outfile");
    }
}

uintmax_t
dump_remainder (
    const char *filename,
    int fd,
    uintmax_t n_bytes,
    FILE *target_stream) {

    uintmax_t n_written = 0;
    uintmax_t n_remaining = n_bytes;

    while (1) {
        char buffer[BUFSIZ];
        size_t n = MIN (n_remaining, BUFSIZ);
        size_t bytes_read = safe_read (fd, buffer, n);
        if (bytes_read == SAFE_READ_ERROR) {
            if (errno != EAGAIN)
                LOG_ERROR("error reading %s",filename);
            break;
        }
        if (bytes_read == 0)
            break;
        write_out_stream (buffer, bytes_read, target_stream);
        n_written += bytes_read;
        if (n_bytes != COPY_TO_EOF) {
            n_remaining -= bytes_read;
            if (n_remaining == 0 || n_bytes == COPY_BUFFER)
                break;
        }
    }

    return n_written;
}


static bool
file_lines (
    int fd,
    uintmax_t n_lines,
    off_t start_pos,
    off_t end_pos,
    uintmax_t *read_pos,
    const char *filename,
    FILE *out) {

    if (n_lines == 0)
        return true;

    char buffer[BUFSIZ];
    off_t pos = end_pos;

    /* Set 'bytes_read' to the size of the last, probably partial, buffer; 0 < 'bytes_read' <= 'BUFSIZ'.  */

    size_t bytes_read = (pos - start_pos) % BUFSIZ;
    if (bytes_read == 0)
        bytes_read = BUFSIZ;

    /* Make 'pos' a multiple of 'BUFSIZ' (0 if the file is short), so that all reads will be on block boundaries, which might increase efficiency */

    pos -= bytes_read;
    xlseek (fd, pos, SEEK_SET, filename);
    bytes_read = safe_read (fd, buffer, bytes_read);
    if (bytes_read == SAFE_READ_ERROR) {
        LOG_ERROR("error reading %s", filename);
        return false;
    }
    *read_pos = pos + bytes_read;

    /* Count the incomplete line on files that don't end with a newline.  */
    if (bytes_read && buffer[bytes_read - 1] != '\n')
        --n_lines;

    do {
        /* Scan backward, counting the newlines in this bufferfull.  */
        size_t n = bytes_read;
        while (n) {
            const char *nl = memrchr (buffer, '\n', n);
            if (nl == NULL)
                break;
            n = nl - buffer;
            if (n_lines-- == 0) {
                /* If this newline isn't the last character in the buffer,
                   output the part that is after it.  */
                if (n != bytes_read - 1)
                    write_out_stream (nl + 1, bytes_read - (n + 1), out);
                *read_pos += dump_remainder (filename, fd, end_pos - (pos + bytes_read), out);
                return true;
            }
        }

        /* Not enough newlines in that bufferfull.  */
        if (pos == start_pos) {
            /* Not enough lines in the file; print everything from
               start_pos to the end.  */
            xlseek (fd, start_pos, SEEK_SET, filename);
            *read_pos = start_pos + dump_remainder (filename, fd, end_pos, out);
            return true;
        }
        pos -= BUFSIZ;
        xlseek (fd, pos, SEEK_SET, filename);

        bytes_read = safe_read (fd, buffer, BUFSIZ);
        if (bytes_read == SAFE_READ_ERROR) {
            LOG_ERROR("error reading %s", filename);
            return false;
        }

        *read_pos = pos + bytes_read;
    }
    while (bytes_read > 0);

    return true;
}

static bool
pipe_lines (
    int fd,
    uintmax_t n_lines,
    uintmax_t *read_pos,
    const char *filename,
    FILE *out) {

    typedef struct linebuffer {
        char buffer[BUFSIZ];
        size_t nbytes;
        size_t nlines;
        struct linebuffer *next;
    } LBUFFER;

    LBUFFER *first, *last, *tmp;
    size_t total_lines = 0;	/* Total number of newlines in all buffers */
    bool ok = true;
    size_t n_read;		/* Size in bytes of most recent read */

    first = last = xmalloc (sizeof (LBUFFER));
    first->nbytes = first->nlines = 0;
    first->next = NULL;
    tmp = xmalloc (sizeof (LBUFFER));

    /* Input is always read into a fresh buffer */
    while (1) {
        n_read = safe_read (fd, tmp->buffer, BUFSIZ);
        if (n_read == 0 || n_read == SAFE_READ_ERROR)
            break;
        tmp->nbytes = n_read;
        *read_pos += n_read;
        tmp->nlines = 0;
        tmp->next = NULL;

        /* Count the number of newlines just read */
        {
            char const *buffer_end = tmp->buffer + n_read;
            char const *p = tmp->buffer;
            while ((p = memchr (p, '\n', buffer_end - p))) {
                ++p;
                ++tmp->nlines;
            }
        }
        total_lines += tmp->nlines;

        /* If there is enough room in the last buffer read, just append the new one to it.  This is because when reading from a pipe, 'n_read' can often be very small */
        if (tmp->nbytes + last->nbytes < BUFSIZ) {
            memcpy (&last->buffer[last->nbytes], tmp->buffer, tmp->nbytes);
            last->nbytes += tmp->nbytes;
            last->nlines += tmp->nlines;
        }
        else {
            /*
               If there's not enough room, link the new buffer onto the end of the list, then either free up the oldest buffer for the next read if that would leave enough lines, or else malloc a new one. Some compaction mechanism is possible but probably not worthwhile */
            last = last->next = tmp;
            if (total_lines - first->nlines > n_lines) {
                tmp = first;
                total_lines -= first->nlines;
                first = first->next;
            }
            else
                tmp = xmalloc (sizeof (LBUFFER));
        }
    }

    free (tmp);

    if (n_read == SAFE_READ_ERROR) {
        LOG_ERROR("error reading %s", filename);
        ok = false;
        goto free_lbuffers;
    }

    /* If the file is empty, then bail out */
    if (last->nbytes == 0)
        goto free_lbuffers;

    /* This prevents a core dump when the pipe contains no newlines */
    if (n_lines == 0)
        goto free_lbuffers;

    /* Count the incomplete line on files that don't end with a newline */
    if (last->buffer[last->nbytes - 1] != '\n') {
        ++last->nlines;
        ++total_lines;
    }

    /* Run through the list, printing lines.  First, skip over unneeded buffers */
    for (tmp = first; total_lines - tmp->nlines > n_lines; tmp = tmp->next)
        total_lines -= tmp->nlines;

    /* Find the correct beginning, then print the rest of the file */
    {
        char const *beg = tmp->buffer;
        char const *buffer_end = tmp->buffer + tmp->nbytes;
        if (total_lines > n_lines) {
            /* Skip 'total_lines' - 'n_lines' newlines.  We made sure that 'total_lines' - 'n_lines' <= 'tmp->nlines' */
            size_t j;
            for (j = total_lines - n_lines; j; --j) {
                beg = memchr (beg, '\n', buffer_end - beg);
                assert (beg);
                ++beg;
            }
        }

        write_out_stream (beg, buffer_end - beg, out);
    }

    for (tmp = tmp->next; tmp; tmp = tmp->next)
        write_out_stream (tmp->buffer, tmp->nbytes, out);

free_lbuffers:
    while (first) {
        tmp = first->next;
        free (first);
        first = tmp;
    }
    return ok;
}

static bool
flush (
    int fd,
    uintmax_t *read_init_ofs,
    const char *filename,
    FILE *out) {

    struct stat file_stat;
    if (0 > fstat(fd, &file_stat))
        return false;
    else {

        off_t begin_ofs = -1;
        off_t end_ofs;

        if (CHECK_WORTH(file_stat.st_mode)
            && (begin_ofs = lseek(fd, 0, SEEK_CUR)) != -1
            && begin_ofs < (end_ofs = lseek(fd, 0, SEEK_END))) {

            *read_init_ofs = end_ofs;
            if (end_ofs !=0
                && !file_lines(fd, LN_GUESS, begin_ofs, end_ofs, read_init_ofs, filename, out))
                return false;
        } else {
            if (begin_ofs != -1) {
                xlseek(fd, begin_ofs, SEEK_SET, filename); //ignore error
            }
            return pipe_lines (fd, LN_GUESS, read_init_ofs, filename, out);
        }
    }
    return true;
}


static bool
flush_margin_growth (
    struct logz_file_def *filedef,
    FILE *out) {

    int fd = open(filedef->name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (-1 == fd) {
        filedef->proc_worth = false;
        filedef->fd = -1;
        filedef->ignore = false;
        filedef->errnum = errno;
        filedef->dev = 0;
        filedef->ino = 0;

        LOG_ERROR("failed to open file:%s for reading", filedef->name);

        logz_close_fd(fd, filedef->name);
        return false;
    } else {
        uintmax_t read_init_ofs;
        // write_mast_head(out, filedef->name); // @multi

        // flush before we stat again
        if (! flush (fd, &read_init_ofs, filedef->name, out)) {
            logz_close_fd(fd, filedef->name);
            return false;
        }

        struct stat file_stat;
        if (0 > lzsleep(1))
            LOG_ERROR("immd sleep failed. expect log truncated at top or record when init");

        errno = 0;
        if (0 > fstat(fd, &file_stat)) {
            filedef->errnum = errno;

            LOG_ERROR("failed to read file:%s", filedef->name);

            logz_close_fd(fd, filedef->name);
            return false;
        }
        if (!CHECK_WORTH(file_stat.st_mode)) {
            filedef->errnum = -1;
            filedef->ignore = true;

            LOG_ERROR("file:%s is not a regular file. will not be collected from!", filedef->name);

            logz_close_fd(fd, filedef->name);
            return false;
        } else {
            logz_remember_fd(filedef, fd, read_init_ofs, &file_stat, 1);
        }
    }
    return true;

}

int
attach_observe_events (
    size_t num_files,
    char **files,
    char *target) {

    header = TOGGLE;
    if (num_files <= 1)
        header = OFF;

    size_t i = 0;
    bool health = true;

    struct logz_file_def *filedef = xnmalloc(num_files, sizeof(*filedef));
    for (i = 0; i < num_files; i++)
        filedef[i].name = files[i];

// open target for writing
    errno = 0;
    FILE *target_stream = fopen(target, "a+");
    if (0 != errno)
        return LOG_ERROR("cannot open file:%s for writing", target), -1;

    for(i = 0; i < num_files; i++)
        health &= flush_margin_growth(&filedef[i], target_stream);

#if HAVE_INOTIFY
    if ( any_symlinks (filedef, num_files)
        || (!health)) {
        LOG_ERROR("%s","no suitable stream to observe. make sure source files exist and are not symlinks");
        exit(EXIT_SUCCESS);
    }

    int wd = inotify_init ();
    if (0 <= wd) {
        if (fflush (target_stream) != 0) {
            LOG_ERROR("failed initial flush on target! exiting!!");
            exit(EXIT_FAILURE);
        }

        if (!logz_recursive_inotify (wd, filedef, num_files, target_stream))
            exit (EXIT_FAILURE);
    } else {
        return LOG_ERROR("%s", "we need inotify!"), -1;
    }
#endif
    return 0;
}
