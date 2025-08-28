include ../Makefile.in
CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))
.PHONY:all clean install

all: install

clean:
	$(Q)rm -rf ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/resource
	$(Q)echo clean sample_gpu_2d/resource
install:
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	cp -raf ${PLATFORM_SAMPLES_DIR}/${RELATIVE_PATH}/resource \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	$(Q)echo install sample_gpu_2d/resource

distclean:
	$(Q)rm -rf ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/resource
	$(Q)echo distclean up installed sample_gpu_2d/resource
