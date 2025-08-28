include ../../Makefile.in

CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))

TARGET := uvc_capture_sample

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

INCS += ./

LIBSDIR += -L../libs
LIBS += -levent_pthreads -lhbplayer -lpthread -levent -lalog -lrt

.PHONY: all clean install

%.o: %.c
	@mkdir -p $(abspath $(dir $@))
	$(CC) -std=c11 -Wall $(INCS) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) -std=c11 -Wall -O0 -o $@ $(OBJS) $(LIBSDIR) $(LIBS) $(CFLAGS)

all: $(TARGET)

clean:
	rm -rf $(TARGET) *.o

install: $(TARGET)
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	$(Q)install -m 0775 ${TARGET} \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
