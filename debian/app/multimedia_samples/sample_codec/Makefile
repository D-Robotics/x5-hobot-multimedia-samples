include ../Makefile.in
CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))

TARGET = sample_codec

SRCS := $(foreach cf, $(SRC_PATH), $(wildcard $(cf)/*.c))
OBJS :=  $(SRCS:.c=.o)

INCS += -I ${HR_BUILD_OUTPUT_DIR}/deploy/system/usr/include/
LIBS += -lmultimedia -lavformat -lavcodec -lavutil -lssl -lcrypto -ldrm -lz

.PHONY: all clean install

%.o:%.c
	@mkdir -p $(abspath $(dir $@))
	$(CC) -o $@ $(INCS) $(CFLAGS) -c $<

$(TARGET):$(OBJS)
	@mkdir -p $(abspath $(dir $@))
	$(CC) -O0 -o $@ $(OBJS) $(CFLAGS) ${LIBSDIR} $(LIBS)

all: ${TARGET}

clean:
	rm -rf ${OBJS} ${TARGET}

install: ${TARGET}
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	$(Q)install -m 0775 ${TARGET} \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
	$(Q)install -m 0664 *.yuv codec_config.ini *.h264 \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
