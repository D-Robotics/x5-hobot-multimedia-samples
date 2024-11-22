include ../Makefile.in
CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))

.PHONY: default
default:
	@echo "This is the default target. It does nothing."

clean:
	rm -rf ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/resource
	rm -f ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/insmode_driver.sh

install:
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	$(Q)install -m 0775 insmode_driver.sh \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
	$(Q)cp -raf ${CUR_DIR}/resource \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/