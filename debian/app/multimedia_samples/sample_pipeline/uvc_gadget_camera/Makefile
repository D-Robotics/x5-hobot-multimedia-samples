include ../../Makefile.in
CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))

TARGET = uvc_gadget_camera
AUDIO_FILE = audio/count10_48K_chn2_s16le.pcm

SRCS := $(foreach cf, $(SRC_PATH), $(wildcard $(cf)/*.c))
OBJS :=  $(SRCS:.c=.o)
INCS += -I ${HR_BUILD_OUTPUT_DIR}/deploy/system/usr/include/

.PHONY: all clean install

LIBSDIR += -L./lib
INCS += -I./include
LIBS += -lguvc -lmultimedia -lasound -lpthread
%.o:%.c
	@mkdir -p $(abspath $(dir $@))
	$(CC) $(INCS) $(CFLAGS) -c $< $(LIBS) $(LIBSDIR) -o $@


$(TARGET):$(OBJS)
	@mkdir -p $(abspath $(dir $@))
	$(CC) -O0 -o $@ $(OBJS) $(CFLAGS) ${LIBSDIR} $(LIBS)

all: ${TARGET}

clean:
	rm -rf ${OBJS} ${TARGET} install


install: ${TARGET}
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	$(Q)install -m 0775 ${TARGET} \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
	$(Q)install -m 0775 ${AUDIO_FILE} \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
