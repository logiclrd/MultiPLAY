default: release

release: FORCE
	@make -f build/target.$@ $@

debug: FORCE
	@make -f build/target.$@ $@

lint: FORCE
	@make -f build/target.$@ $@

bare: FORCE
	@make -f build/target.$@ $@

clean: FORCE
	rm -rf obj/ bin/ tests/actual

-include tests/target.test

FORCE:

.PHONY: default release debug lint bare clean FORCE
