PWD=$(shell pwd)
GLPK_VERSION=4.47

buildall:
	@$(MAKE) lib/inotify_tools.a

inotify_tools/Makefile:
	cd inotify_tools; ./autogen.sh && ./configure --prefix=$(PWD)

lib/inotify_tools.a:
	@$(MAKE) inotify_tools/Makefile
	@$(MAKE) -C inotify_tools install

clean:
	@echo "not cleaning third_party libs"

include ../core/make/logzilla.mk
