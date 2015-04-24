SRC=defines.c bitrotate.c hash.c argmatch.c closein.c closeout.c \
	c-strtod.c dirname.c exitfail.c fcntl.c isapipe.c \
	msvc-inval.c msvc-nothrow.c posixver.c localcharset.c \
	quotearg.c safe-read.c safe-write.c timespec.c stat-time.c xfreopen.c \
	xmalloc.c xnanosleep.c xstrtod.c xstrtol.c fd-safer.c dup-safer.c \
	open-safer.c close-stream.c logger.c logz.c logz_inotify_collector.c

CFLAGS+= -I ../include
