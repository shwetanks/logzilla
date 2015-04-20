TARGET=logdaemon

SRC=

LDFLAGS+=-L../../core/lib -llogzilla -pthread -lm -lz -ldl -lrt
LDFLAGS+=-L ../../third_party/lib
LDFLAGS+=-L../../contrib/lib -lcontrib

CFLAGS+= -I ../../core/include -I ../../contrib/include -I ../include -I .

include ../../core/make/logzilla.mk
