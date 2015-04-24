#ifndef _LOGZILLA_H_
#define _LOGZILLA_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef _GNU_SOURCE
#define __USE_GNU 1
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define likely(x)     __builtin_expect((x),1)
#define unlikely(x)   __builtin_expect((x),0)

#define _LOGZ_INLINE_ static inline

#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>

#include "strutil.h"
#include "logger.h"

#endif
