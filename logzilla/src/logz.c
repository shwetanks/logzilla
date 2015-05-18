#include "logz_config.h"
#include <stdio.h>
#include <sys/inotify.h>
#include <stdbool.h>
#include <libgen.h>
#include <sys/stat.h>
#include "http_client_pool.h"
#include "logz_utils.h"
#include "uri_encode.h"
#include "json.h"


struct logdaemon_config logconf = LOGDAEMON_INITIALIZER;
bool write_to_file = false;

/* this server */
static char *hostname = NULL;

/* server where we push data, if specified */
struct server eserv;

#define MAX_FILE_SUPPORT 10
#define HTTP_CLIENT_TIMEOUT 60000

struct http_client_pool client_pool = {
    .timeout_handler.timeout = HTTP_CLIENT_TIMEOUT,
    .timeout_handler_persistent.timeout = HTTP_CLIENT_TIMEOUT
};


struct logz_file_def {
    char *name;            /* file name */
    off_t size;            /* file size */
    struct timespec mtime;
    mode_t mode;           /* file mode */
    int fd;
    int errnum;            /* errno on last check */
    int wd;                /* inotify internal */
    int parent_wd;         /* on parent directory inotify internal */
    size_t basename_start; /* basename offs in filename  */
};

static const uint32_t inotify_file_watch_mask = (IN_MODIFY | IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF);

#define COPY_TO_EOF UINTMAX_MAX
#define INTERFACE_ONERROR_RETRY_THRESHOLD 2
int success = 0, failure = 0;

struct thashtable *tab_event_fds;
struct thashtable *delta_push;

struct vmbuf write_buffer = VMBUF_INITIALIZER;
struct vmbuf mb = VMBUF_INITIALIZER;
static struct file_writer fw;


static int
timecmp (struct timespec a, struct timespec b) {
    return (a.tv_sec < b.tv_sec ? -1
            : a.tv_sec > b.tv_sec ? 1
            : (int) (a.tv_nsec - b.tv_nsec));
}

struct timespec
mtime_to_spec (const struct stat *st) {
    struct timespec time;
    time.tv_sec = st->st_mtim.tv_sec;
    time.tv_nsec = st->st_mtim.tv_nsec;
    return time;
}

void
dump_stats () {
    LOGGER_INFO("stats since alive:: success:%d | failures:%d", success, failure);
}


void
logz_close_fd (int fd, const char *filename) {
    if (-1 != fd && close (fd))
        LOGGER_ERROR("failed to close file:%s (%d)", filename, fd);
}

static int
http_client_pool_post_request2(
    struct http_client_pool *http_client_pool,
    struct in_addr addr, uint16_t port, const char *hostname,
    const char *data, size_t size_of_data, const char *format, ...) {

    struct http_client_context *cctx = http_client_pool_create_client2(http_client_pool, addr, port, hostname, NULL);
    if (NULL == cctx)
        return -1;
    vmbuf_reset(&cctx->request);
    vmbuf_strcpy(&cctx->request, "POST ");
    va_list ap;
    va_start(ap, format);
    vmbuf_vsprintf(&cctx->request, format, ap);
    va_end(ap);
    vmbuf_sprintf(&cctx->request, " HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", hostname, size_of_data);
    vmbuf_memcpy(&cctx->request, data, size_of_data);
    vmbuf_chrcpy(&cctx->request, '\0');
    if (0 > http_client_send_request(cctx))
        return http_client_free(cctx), -1;
    return 0;
}

static int
post_to_interface (char *data, size_t data_len) {

    if (0 > http_client_pool_post_request2(&client_pool, eserv.server, eserv.port, eserv.hostname, data, data_len, "%s/%s", eserv.context, eserv.hostname)) {
        return LOGGER_ERROR("failed to send request to %s", eserv.hostname), -1;
    }

    yield();
    struct http_client_context *rcctx = http_client_get_last_context();
    if (rcctx->http_status_code != 201) {
        ++failure;
        http_client_free(rcctx);
        return LOGGER_ERROR("request failed with code %d", rcctx->http_status_code), -1;
    }
    ++success;
    return http_client_free(rcctx), 0;
}

static void
write_out_stream (const char *filename, char *data) {
    vmbuf_reset(&write_buffer);
    vmbuf_sprintf(&write_buffer, "{ \"message\": \"%s|%s|", hostname, filename);
    json_escape_str_vmb(&write_buffer, data);
    vmbuf_strcpy(&write_buffer, "\" }");
    vmbuf_chrcpy(&write_buffer, '\0');

    if (write_to_file) {
        if (0 > file_writer_write(&fw, vmbuf_data(&write_buffer), vmbuf_wlocpos(&write_buffer))) {
            LOGGER_ERROR("%s", "failed write attempt on outfile| aborting to diagnose!");
            abort();
        }
        return;
    }

    int threshold = INTERFACE_ONERROR_RETRY_THRESHOLD;
    while (0 > post_to_interface(vmbuf_data(&write_buffer), vmbuf_wlocpos(&write_buffer)) && (0 < threshold--)) {
        if (0 == post_to_interface(vmbuf_data(&write_buffer), vmbuf_wlocpos(&write_buffer))) {
            LOGGER_ERROR("post failed to %s, issuing reattempt#%d", eserv.hostname, threshold);
            --failure;
            break;
        }
    }
}


static char *
write_file_fringe (const char *filename, char *data, int fd) {
    thashtable_rec_t *rec = thashtable_lookup(delta_push, &fd, sizeof(fd));
    struct vmbuf delta = *(struct vmbuf *)thashtable_get_val(rec);
    char *past = vmbuf_data(&delta);
    char *lookahead = NULL;

    if (!SSTRISEMPTY(past)) {
        lookahead = strchr(data, '\n');
        char *trailing_past = ribs_malloc_sprintf("%.*s", ((int)strlen(data) - (int)strlen(lookahead)), data);
        if (trailing_past) {
            char *d_composite = ribs_malloc_sprintf("%s%s", past, trailing_past);
            write_out_stream(filename, d_composite);
            d_composite += strlen(d_composite); // advance by that
            vmbuf_reset(&delta);
        }
    }
    return lookahead;
}

static void
trigger_writer (
    const char *filename,
    struct logz_file_def *filedef) {

    vmbuf_reset(&write_buffer);
    ssize_t res;
    char *fn = basename(ribs_strdup(filename));
    while(1) {
        vmbuf_reset(&write_buffer);
        res = read(filedef->fd, vmbuf_wloc(&write_buffer), (BUFSIZ + 1024) &~ 1024);

        filedef->size += res;
        lseek (filedef->fd, filedef->size, SEEK_SET);

        if (0 > vmbuf_wseek(&write_buffer, res)) {
            LOGGER_ERROR("%s", "wseek error");
            break;
        } else if (0 > res) {
            LOGGER_ERROR("%s", "read error"); // EAGAIN is handled by poller
            break;
        } else if (0 < res) {
            // initial sanitizer
            vmbuf_chrcpy(&write_buffer, '\0'); // kill garbage
            char *data = ribs_strdup(vmbuf_data(&write_buffer));
            //data = strchr(data, '\n') + 1; // skip broken data from initial buffer start. we read from where the file was first observed
            ssize_t write_depth = res = strlen(data);
            // line doesn't end here
            if (data[res - 1] != '\n') {
                char *datafringe = ribs_strdup((char *)memrchr(data, '\n', res));
                if (SSTRISEMPTY(datafringe))
                    break;
                write_depth = strlen(data) - strlen(datafringe);
                *(data + write_depth) = 0;

                if (filedef->size != 0) {
                    char *rebalanced_data = write_file_fringe(fn, data, filedef->fd);
                    if (NULL != rebalanced_data) {
                        data = rebalanced_data;
                        write_depth = strlen(data);
                    }
                }

                thashtable_rec_t *rec = thashtable_lookup(delta_push, &filedef->fd, sizeof(filedef->fd));
                struct vmbuf kdelta = *(struct vmbuf *)thashtable_get_val(rec);
                vmbuf_strcpy(&kdelta, datafringe);
                vmbuf_chrcpy(&kdelta, '\0');
            }

            vmbuf_reset(&write_buffer);
            vmbuf_memcpy(&write_buffer, data, write_depth);
            vmbuf_chrcpy(&write_buffer, '\0');
            write_out_stream(fn, ribs_strdup(vmbuf_data(&write_buffer)));
        } else if (0 == res) {
            break;
        }
    }
}


static void
_flush (
    struct logz_file_def *filedef,
    int wd,
    int *prev_wd) {

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
        LOGGER_ERROR("%s: file truncated", name);
        *prev_wd = wd;
        lseek (filedef->fd, stats.st_size, SEEK_SET);
        filedef->size = stats.st_size;
    } else if (S_ISREG (filedef->mode)
               && stats.st_size == filedef->size
               && timecmp (filedef->mtime, mtime_to_spec(&stats)) == 0)
        return;

    if (wd != *prev_wd) {
        *prev_wd = wd;
    }

    trigger_writer (name, filedef);
}


static bool
recursive_flush_events (
    int inotify_wd,
    char **files,
    uint32_t num_files) {

    struct logz_file_def *filedef = ribs_malloc(num_files * sizeof(struct logz_file_def));

    int prev_wd;
    size_t evlen = 0;
    // size_t evbuf_off = 0;
    
    bool found_unwatchable_dir = false;
    bool no_inotify_resources = false;

    int inserted = 0;

    size_t i;
    for (i = 0; i < num_files; i++) {

        filedef[i].name = files[i];
        size_t fnlen = strlen (filedef[i].name);
        if (evlen < fnlen)
            evlen = fnlen;

        filedef[i].wd = -1;
        char *file_fullname = ribs_strdup(filedef[i].name);
        char *dir_name = dirname(file_fullname);
        size_t dirlen = strlen(dir_name);;
        char prev = filedef[i].name[dirlen];
        filedef[i].basename_start = basename (file_fullname) - filedef[i].name;

        filedef[i].name[dirlen] = '\0';

        filedef[i].parent_wd = inotify_add_watch(inotify_wd, dir_name, (IN_CREATE | IN_MOVED_TO));

        filedef[i].name[dirlen] = prev;

        if(filedef[i].parent_wd < 0) {
            if (errno != ENOSPC)
                LOGGER_ERROR("cannot watch parent directory of file %s", filedef[i].name);
            else {
                no_inotify_resources = true;
                LOGGER_ERROR("%s", "inotify resources exhausted");
            }
            found_unwatchable_dir = true;
            break;
        }

        filedef[i].wd = inotify_add_watch(inotify_wd, filedef[i].name, inotify_file_watch_mask);

        if (filedef[i].wd < 0) {
            if (errno == ENOSPC) {
                no_inotify_resources = true;
                LOGGER_ERROR("%s", "inotify resources exhausted");
            } else if(errno != filedef[i].errnum)
                LOGGER_ERROR("cannot watch %s", filedef[i].name);
            continue;
        }
        filedef[i].fd = open(filedef[i].name, O_RDONLY | O_NONBLOCK);
        if (0 >= filedef[i].fd) {
            LOGGER_ERROR("skipping file %s. cannot open to read", filedef[i].name);
            continue;
        }

        struct stat stats;
        if (fstat (filedef[i].fd, &stats) != 0) {
            LOGGER_ERROR("skipping file %s.cannot stat", filedef[i].name);
            filedef->errnum = errno;
            logz_close_fd (filedef[i].fd, filedef[i].name);
            filedef->fd = -1;
            continue;
        }
        filedef[i].size = stats.st_size;
        lseek (filedef[i].fd, 0, SEEK_END); // no offset enforced

        thashtable_insert(tab_event_fds, &filedef[i].wd, sizeof(filedef[i].wd), &filedef[i], sizeof(filedef[i]), &inserted);

        struct vmbuf kdelta = VMBUF_INITIALIZER;
        vmbuf_init(&kdelta, 4096);

        thashtable_insert(delta_push, &filedef[i].fd, sizeof(filedef[i].fd), &kdelta, sizeof(kdelta), &inserted);
    }

    if(no_inotify_resources || found_unwatchable_dir) {
        LOGGER_ERROR("%s", "running low on inotify resources / got an unwatchable directory. Aborting!!");
        abort();
    }

    prev_wd = filedef[num_files -1].wd;

    evlen += sizeof (struct inotify_event) + 1;
    struct vmbuf evbuf = VMBUF_INITIALIZER;
    vmbuf_init(&evbuf, evlen);

    ssize_t res = 0;
    struct timeval delay; /* how long to wait for file changes.  */
    delay.tv_sec = (time_t) 0.50;
    delay.tv_usec = 1000000 * (0.50 - delay.tv_sec);

    fd_set rfd;
    FD_ZERO (&rfd);
    FD_SET (inotify_wd, &rfd);

    while(1) {
        if (thashtable_get_size(tab_event_fds) == 0) {
            LOGGER_INFO("%s", "no file to read");
            return true;
        }

        {
            int file_change = select(inotify_wd + 1, &rfd, NULL, NULL, NULL);

            if (file_change == 0)
                continue;
            else if (file_change == -1) {
                LOGGER_ERROR("%s", "error monitoring inotify event");
                exit(EXIT_FAILURE);
            }

            vmbuf_reset(&evbuf);

            while (0 < (res = read(inotify_wd, vmbuf_wloc(&evbuf), evlen))) {
                if (0 > vmbuf_wseek(&evbuf, res))
                    return false;
                else if (errno == EINTR)
                    continue;
                else if (0 < res)
                    break;
            }
            if (errno == EAGAIN && res < 0)
                continue;
            // res might be 0, or could have overrun memory
            if (res == 0) {
                LOGGER_ERROR("%s", "error reading inotify event|bad buffer size. aborting to investigate");
                abort();
            }
            // another case when res == 0 or could have overrun memory or got EINVAL
            // -- ignored --
            // what to do if? .. realloc buffer. but the deal is we're on vmbuf which will grow if world is that bad. we're mostly safe here. hence ignored.
        }

        struct logz_file_def *tmp;
        struct inotify_event *event = (struct inotify_event *)vmbuf_data(&evbuf);
        if (event->len) {
            // events of lower interest?. these are from watched directory. we'll drop those which we're not watching for and will set watch on those of interest.
            size_t x;
            for (x = 0; x < num_files; x++) {
                if (filedef[x].parent_wd == event->wd
                    && strcmp (event->name, filedef[x].name + filedef[x].basename_start))
                    break;
            }
            if (x == num_files)
                continue;
            int wdx = inotify_add_watch (inotify_wd, filedef[x].name, inotify_file_watch_mask);
            if (0 > wdx) {
                LOGGER_ERROR("cannot watch %s", filedef[x].name);
                continue;
            }
            tmp = &(filedef[x]);
            thashtable_remove(tab_event_fds, &tmp->wd, sizeof (tmp->wd));
            tmp->wd = wdx;
            thashtable_insert(tab_event_fds, &tmp->wd, sizeof(tmp->wd), &tmp, sizeof(struct logz_file_def), &inserted);
            // rebalance new found file | make all assertions | we'll read from this as well.
            UNUSED(tmp);
        } else {
            thashtable_rec_t *rec = thashtable_lookup(tab_event_fds, &event->wd, sizeof(event->wd));
            tmp = (struct logz_file_def *)thashtable_get_val(rec);
        }
        if (!tmp) {
            continue;
        }

        if (event->mask & (IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF)) {
            if (event->mask & IN_DELETE_SELF) {
                inotify_rm_watch(inotify_wd, tmp->wd);
                thashtable_remove(tab_event_fds, &tmp->wd, sizeof(tmp->wd));
            }
            continue;
        }
        _flush(tmp, event->wd, &prev_wd);
    }
    return true;
}

int main(int argc, char *argv[]) {

    size_t num_files = 0;

    char **files = (char **) calloc(MAX_FILE_SUPPORT, MAX_FILE_SUPPORT * sizeof(char *));

    if (0 > init_log_config(&logconf, argc, argv)) {
        exit(EXIT_FAILURE);
    }
    if (!SSTRISEMPTY(logconf.target) && !SSTRISEMPTY(logconf.interface)) {
        LOGGER_ERROR("%s", "cannot write to target and interface together. please choose one");
        exit(EXIT_FAILURE);
    }
        

    char *f = logconf.watch_files;
    if (!SSTRISEMPTY(f)) {
        while (f != NULL) {
            char *fprime = strsep(&f, ",");
            if (fprime != NULL) {
                files[num_files] = strdup(fprime);
                ++num_files;
            }
        }
    } else {
        LOGGER_ERROR("%s", "no files..no watch!");
        exit(EXIT_FAILURE);
    }

    if (0 > epoll_worker_init()) {
        LOGGER_ERROR("%s", "epoll_worker_init");
        exit(EXIT_FAILURE);
    }

    ribs_timer(60*1000, dump_stats);

    tab_event_fds = thashtable_create();
    delta_push    = thashtable_create();
    vmbuf_init(&write_buffer, 4096);
    vmbuf_init(&mb, 4096);


    if (SSTRISEMPTY(logconf.interface) && !SSTRISEMPTY(logconf.target)) {
        file_writer_make(&fw);

        if (0 > file_writer_init(&fw, logconf.target)) {
            LOGGER_ERROR("%s", "flie_writer");
            exit(EXIT_FAILURE);
        }
        write_to_file = true;
    } else if (!SSTRISEMPTY(logconf.interface)) {

        if (0 > http_client_pool_init(&client_pool, 20, 20)) {
            LOGGER_ERROR("http_client_pool_init");
            exit(EXIT_FAILURE);
        }

        memset(&eserv, 0, sizeof(eserv));

        vmbuf_reset(&write_buffer);
        _replace(logconf.interface, &write_buffer, "http://www.", "");
        _replace(logconf.interface, &write_buffer, "http://", "");

        char *interface = vmbuf_data(&write_buffer);
        eserv.context = ribs_strdup(strstr(interface, "/"));
        
        char *ln = strchr(interface, '/');
        ln = ribs_malloc_sprintf("%.*s", ((int)strlen(interface) - (int)strlen(ln)), interface);

        if (0 > parse_host_to_inet(ln, eserv.hostname, &eserv.server, &eserv.port)) {
            LOGGER_ERROR("%s", "server details invalid. cannot parse server");
            exit(EXIT_FAILURE);
        }
    } else {
        LOGGER_ERROR("%s", "no target defined. please use target or interface");
        exit(EXIT_FAILURE);
    }


    char _hostname[1024];
    gethostname(_hostname, 1024);
    hostname = ribs_strdup(_hostname);

    int wd = inotify_init1(IN_NONBLOCK);
    if (0 >= wd) {
        LOGGER_ERROR("%s", "failed to init inotify. cannot proceed. make sure you've inotify and is accessible to this user.");
        exit(EXIT_FAILURE);
    }

    if (!recursive_flush_events(wd, files, num_files)) {
        LOGGER_ERROR("%s", "collection failed");
        abort();
    }

    return 0;
}
