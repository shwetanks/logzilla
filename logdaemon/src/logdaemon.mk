TARGET=logdaemon

SRC=logdaemon.c

CFLAGS+= -I ../../core/include -I ../include -I .
LDFLAGS+=-lm -lz -ldl -L../../core/lib -llogzilla -lrt

include ../../core/make/logzilla.mk
