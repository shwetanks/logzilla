all:
	@$(MAKE) -s $(LIB_BUILDS) $(SUBDIR_BUILDS) $(DIR_BUILDS)

.PHONY: $(LIB_BUILDS) $(SUBDIR_BUILDS) $(DIR_BUILDS) liblogzilla

liblogzilla:
	@echo "[$@] build"
	@$(MAKE) -s -C core/src
	@echo "[$@] success"

liblogzilla_clean:
	@echo "[$(@:%_clean=%)] clean"
	@$(MAKE) -s -C core/src clean

clean: $(SUBDIR_BUILDS:%=%__clean) $(LIB_BUILDS:%=%__clean) $(DIR_BUILDS:%=%__clean) liblogzilla_clean

$(SUBDIR_BUILDS): $(LIB_BUILDS) $(DIR_BUILDS)

$(DIR_BUILDS:%=%): liblogzilla
	@echo "(SUBDIR) [$@]"
	@$(MAKE) -s -C $@

$(DIR_BUILDS:%=%__clean):
	@echo "(SUBDIR) [$(@:%__clean=%)]"
	@$(MAKE) -s -C $(@:%__clean=%) clean

$(SUBDIR_BUILDS) $(LIB_BUILDS): liblogzilla
	@echo "(PROJECT) [$(@)]"
	@$(MAKE) -s -C $(@)/src

$(SUBDIR_BUILDS:%=%__clean) $(LIB_BUILDS:%=%__clean):
	@echo "(SUBDIR) [$(@:%__clean=%)]"
	@$(MAKE) -s -C $(@:%__clean=%)/src clean
