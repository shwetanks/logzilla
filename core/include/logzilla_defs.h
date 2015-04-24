#ifndef _LOGZILLA_DEFS_H_
#define _LOGZILLA_DEFS_H_

#include "config.h"
#include <sys/inotify.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>

#define CHECK_WORTH(m) S_ISREG(m)

#define COPY_TO_EOF UINTMAX_MAX
#define COPY_BUFFER (UINTMAX_MAX - 1)


#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif


/* either of:
   file was modified
   metadata changed e.g. permissions, timestamps (might make file inaccessible)
   file/dir under watch was deleted
   file/dir under watch was moved
*/
static const uint32_t inotify_file_watch_mask = (IN_MODIFY | IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF);

#define DEF_PASSIVE_OCYCLES 5

struct logz_file_def {
    char *name;            /* file name */
    off_t size;            /* file size */
    struct timespec mtime;
    dev_t dev;             /* id of device containing file */
    ino_t ino;             /* file serial num */
    mode_t mode;           /* file mode */
    bool ignore;
    bool remote;
    bool proc_worth;
    int fd;
    int errnum;            /* errno on last check */
    int blocking;          /* 0 if O_NONBLOCK set, 1 otherwise, -1 unknown */
    int wd;                /* inotify internal */
    int parent_wd;         /* on parent directory inotify internal */
    size_t basename_start; /* Offset in NAME of the basename part.  */
    uintmax_t passive_threshold; /* watch threshold on passivity */
};

enum LOG_FILE_HEADER {
    ON = 0,
    OFF,
    TOGGLE
};

// assuming watched files grow rapidly
#define LN_GUESS 20
#define LN_WATCH_DELAY 1.0

static inline bool
is_file_valid (struct logz_file_def const *file) {
    return ((1 == file->fd) ^ (0 == file->errnum));
}

static inline int
lzsleep (double seconds) {
    struct timespec req = {seconds, 0};
    for (;;) {
        errno = 0;
        if (0 == nanosleep(&req, NULL))
            break;
        if (errno != 0)
            return -1;
    }
    return 0;
}

void logz_close_fd (int fd, const char *filename);
void logz_remember_fd (struct logz_file_def *file, int fd, off_t size, struct stat const *filestat, int blocking);
off_t xlseek (int fd, off_t offset, int limit, const char *filename);
void write_out_stream (const char *buf, size_t num, FILE *outfile);
uintmax_t dump_remainder (const char *filename, int fd, uintmax_t n_bytes, FILE *target_stream);

#endif /* _LOGZILLA_DEFS_H_ */
