include ../../Makefile.in
CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))

TARGET = single_pipe_vin_isp_vse_vo

#SRC_PATH += ${CUR_DIR}/../common
SRCS := $(foreach cf, $(SRC_PATH), $(wildcard $(cf)/*.c))
SRCS += ${CUR_DIR}/../common/vp_display.c

LIBS += -lm -ldrm
INCS += -I $(CUR_DIR)/../common/
INCS += -I ${HR_BUILD_OUTPUT_DIR}/deploy/system/usr/include/libdrm/
INCS += -I ${HR_BUILD_OUTPUT_DIR}/deploy/system/usr/include/

OBJS :=  $(SRCS:.c=.o)

.PHONY: all clean install


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
