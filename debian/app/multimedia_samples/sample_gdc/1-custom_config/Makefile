include ../../Makefile.in
CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))

.PHONY: all clean install

all:
	@echo "all target does nothing"

clean:
	@echo "clean target does nothing"

install:
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	$(Q)install -m 0664 *_config*.txt \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
