TARGET=logzilla

SRC=logz.c

CFLAGS+= -I ../../ribs2/include -I ../include -I .
LDFLAGS+=-L -pthread -lz -ldl -L../../ribs2/lib -lribs2 -lrt

include ../../ribs2/make/ribs.mk
