include ./Makefile.in

EXCLUDED_DIRS := chip_base_test sunrise_camera
EXCLUDED_DIRS_FLAGS := $(foreach dir,$(EXCLUDED_DIRS), ! -path "*$(dir)*")
SUB_FOLDERS := $(shell find $(PLATFORM_SAMPLES_DIR) -name "Makefile" \
	$(EXCLUDED_DIRS_FLAGS) | sed 's/Makefile//g' | sed '/samples\/$$/d')
.PHONY:all clean install distclean

all: build install

build:
	$(Q)echo ${SUB_FOLDERS}
	$(Q)for dir in ${SUB_FOLDERS}; do \
		make -C $$dir || exit; \
	done
	make -C $(PLATFORM_SAMPLES_DIR)/sunrise_camera || exit;
	$(Q)echo build all samples

clean:
	$(Q)for dir in ${SUB_FOLDERS}; do \
		make -C $$dir clean || exit; \
	done
	make -C $(PLATFORM_SAMPLES_DIR)/sunrise_camera clean || exit;
	$(Q)echo clean all samples

install: build
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}
	cp -raf ${PLATFORM_SAMPLES_DIR}/chip_base_test \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/

	make -C $(PLATFORM_SAMPLES_DIR)/sunrise_camera install || exit;
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/sunrise_camera
	cp -raf ${PLATFORM_SAMPLES_DIR}/sunrise_camera/out/* \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/sunrise_camera/

	$(Q)echo ${SUB_FOLDERS}
	$(Q)for dir in ${SUB_FOLDERS}; do \
		make -C $$dir install || exit; \
	done

	$(Q)echo install all samples

distclean:
	$(Q)rm -rf ${PLATFORM_SAMPLES_DEPLOY_DIR}
	make -C $(PLATFORM_SAMPLES_DIR)/sunrise_camera distclean || exit;
	$(Q)echo cleaned up all installed samples
