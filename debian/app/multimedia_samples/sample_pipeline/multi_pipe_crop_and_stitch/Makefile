include ../../Makefile.in
CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))

TARGET = multi_pipe_crop_and_stitch

SRC_PATH += ${CUR_DIR}/../common
SRCS := $(foreach cf, $(SRC_PATH), $(wildcard $(cf)/*.c $(cf)/*.cpp))

OBJS := $(patsubst %.cpp,%.obj,$(patsubst %.c,%.o,$(SRCS)))
DEP := $(patsubst %.obj,%.dep,$(patsubst %.o,%.d,$(OBJS)))

.PHONY: all clean install

LIBS += -lNano2Dutil -lNano2D -lm -ldrm -ldnn -lcnn_intf -lhbrt_bayes_aarch64 -lstdc++
INCS += -I $(CUR_DIR)/../common/
INCS += -I /usr/include/libdrm/
INCS += -I /usr/include/

%.o:%.c
	@mkdir -p $(abspath $(dir $@))
	$(CC) $(INCS) $(CFLAGS) -c $< $(LIBS) $(LIBSDIR) -o $@  -Wno-error=int-to-pointer-cast -Wno-error=pointer-to-int-cast

%.obj : %.cpp
	$(CXX) $(CXXFLAGS) $(INCS) -MMD -MP -MF"$(@:%.obj=%.dep)" -MT"$(@:%.obj=%.dep)" -c $< -o $@

$(TARGET):$(OBJS)
	@mkdir -p $(abspath $(dir $@))
	$(CC) -O0 -o $@ $(OBJS) $(CFLAGS) -lmultimedia ${LIBSDIR} $(LIBS)

all: ${TARGET}

clean:
	rm -rf ${OBJS} ${TARGET} install

install: ${TARGET}
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	$(Q)install -m 0775 ${TARGET} \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
