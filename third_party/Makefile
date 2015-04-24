PWD=$(shell pwd)
GLPK_VERSION=4.47

buildall:
	@$(MAKE) lib/libev.a

libev/Makefile:
	cd libev; ./autogen.sh && ./configure --prefix=$(PWD)

lib/libev.a:
	@$(MAKE) libev/Makefile
	@$(MAKE) -C libev install

clean:
	@echo "not cleaning third_party libs"

include ../core/make/logzilla.mk
