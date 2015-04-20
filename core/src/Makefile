logzilla_libs=logzilla

logzilla_libs_clean=$(logzilla_libs:%=%_clean)

buildall : $(logzilla_libs)
clean: $(logzilla_libs_clean)

$(logzilla_libs):
	@echo "========[$@]========"
	@$(MAKE) -s -f "$@.mk"

$(logzilla_libs_clean):
	@echo "========[$@]========"
	@$(MAKE) -s -f "$(@:%_clean=%).mk" clean
