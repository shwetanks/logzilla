#include "logger.h"
#include "logzilla.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>

/* extreme cost .. move onto libev and use shared mem */

char log_buf[4096];

static const char *INFO  = "INFO ";
static const char *ERROR = "ERROR ";

#include <unistd.h>
#include <errno.h>

#ifdef EINTR
# define IS_EINTR(x) ((x) == EINTR)
#else
# define IS_EINTR(x) 0
#endif

static void
begin (const char *msg_class) {

    struct tm tm, *tmp;
    struct timeval tv;
    intmax_t usec;
    gettimeofday(&tv, NULL);
    tmp = localtime_r(&tv.tv_sec, &tm);
    usec = tv.tv_usec;
    strftime(log_buf, sizeof(log_buf), "%Y-%m-%d %H:%M:%S", tmp);
    sprintf(log_buf, ".%03jd.%03jd %d %s ", usec / 1000, usec % 1000, getpid(), msg_class);
}

// ???
enum { READ_MAX = INT_MAX & ~8191 };

static inline void
end (int fd) {

    size_t count = strlen(log_buf);
    for (;;) {
        ssize_t result = write (fd, log_buf, count);

        if (0 <= result)
            return;
        else if (IS_EINTR(errno))
            continue;
        else if (errno == EINVAL && READ_MAX < count)
            count = READ_MAX;
        else
            return;
    }
}

void
vlog (
    int fd,
    const char *format,
    const char *msg_class,
    va_list ap) {

    begin(msg_class);
    vsprintf(log_buf, format, ap);
    end(fd);
}

void
zlog(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vlog(STDOUT_FILENO, format, INFO, ap);
    va_end(ap);
}

void
vlog_at (
    int fd,
    const char *filename,
    unsigned int linenum,
    const char *format,
    const char *msg_class,
    va_list ap) {

    begin(msg_class);
    sprintf(log_buf, "[%s:%u]: ", filename, linenum);
    vsprintf(log_buf, format, ap);
    end(fd);
}

void
zerror_at_line (
    const char *filename,
    unsigned int linenum,
    const char *format, ...) {

    va_list ap;
    va_start(ap, format);
    vlog_at(STDERR_FILENO, filename, linenum, format, ERROR, ap);
    va_end(ap);
}

void
zperror_at_line (
    const char *filename,
    unsigned int linenum,
    const char *format, ...) {

    begin(ERROR);
    va_list ap;
    va_start(ap, format);

    sprintf(log_buf, "[%s:%u]: ", filename, linenum);
    vsprintf(log_buf, format, ap);
    char tmp[512];
    sprintf(log_buf, " (%s)", strerror_r(errno, tmp, 512));
    va_end(ap);
    end(STDERR_FILENO);
}
