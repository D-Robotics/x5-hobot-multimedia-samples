include ../makefile.param

subdir_common :=
subdir_common += utils

rel:
	@for dir in $(subdir_common); do \
		$(MAKE) -C $$dir || exit "$$?"; \
	done

clean:
	@for dir in $(subdir_common); do \
		$(MAKE) -C $$dir clean || exit "$$?"; \
	done
