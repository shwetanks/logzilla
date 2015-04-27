all:
	@$(MAKE) -s -C ribs2          # make ribs2 first
	@echo "[logzilla] build"
	@$(MAKE) -s -C logzilla/src # make our project
clean:
	@$(MAKE) -s -C ribs2 clean    # clean ribs2
	@echo "[logzilla] clean"
	@$(MAKE) -s -C logzilla/src clean  # clean our project

