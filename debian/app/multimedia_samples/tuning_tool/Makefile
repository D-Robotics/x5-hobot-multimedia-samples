include ../Makefile.in
CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))

SH_FILE := run_tuning.sh
BUILD_DIR := out
TUNING_BUILD_DIR := ${BUILD_DIR}/tuning_out
TARGET := ${BUILD_DIR}/isp_tuning

SRC_DIRS := ./
SRC_DIRS += ../utils/
SRC_DIRS += $(shell find ../vp_sensors -maxdepth 1 -type d)

SRC_FILES := $(foreach cf, $(SRC_DIRS), $(wildcard $(cf)/*.c))
OBJ_FILES := $(patsubst %.c,${TUNING_BUILD_DIR}/%.o,$(SRC_FILES))
LIBS += -levent_pthreads -lhbplayer -lpthread -levent -lalog -lm
LIBS += -lOpenCL -lVDK -lGAL -lEGL -lVSC -lCLC -lGLESv2 -lSPIRV_viv -ldrm_vivante -ldrm -lgbm -lgbm_viv
# CFLAGS += -D TUNING_DEBUG
# CFLAGS += -D TUNING_API_DELAY_DEBUG

.PHONY: all clean install

all: ${TARGET}

$(TARGET):$(OBJ_FILES)
	@$(shell mkdir -p ${TUNING_BUILD_DIR})
	@$(CC) -O0 -o $@ $(OBJ_FILES) $(CFLAGS) ${LIBSDIR} $(LIBS)

$(TUNING_BUILD_DIR)/%.o: %.c
	@mkdir -p $(abspath $(dir $@))
	@$(CC) $(INCS) $(CFLAGS) -c $< $(LIBS) $(LIBSDIR) -o $@

clean:
	rm -rf ${TUNING_BUILD_DIR}

install: ${TARGET}
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	$(Q)install -m 0775 ${TARGET} ${SH_FILE} \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
