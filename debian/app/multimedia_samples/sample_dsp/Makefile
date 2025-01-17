include ../Makefile.in
CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))

TARGET = sample_dsp

SRCS := $(foreach cf, $(SRC_PATH), $(wildcard $(cf)/*.cpp))
OBJS :=  $(SRCS:.cpp=.o)

LDFLAGS := -L${OUTPUT_HBROOTFS_DIR}/usr/lib ${LDFLAGS}
INCS := -I${OUTPUT_HBROOTFS_DIR}/include $(INCS)
LIBS += -lalog -ldsp -lhbmem -lpthread -ljsoncpp -lhbipcfhal

.PHONY: all clean install

%.o:%.cpp
	@mkdir -p $(abspath $(dir $@))
	$(CXX) -o $@ $(INCS) $(CFLAGS) -c $<

$(TARGET):$(OBJS)
	@mkdir -p $(abspath $(dir $@))
	$(CXX) -O0 -o $@ $(OBJS) $(CFLAGS) ${LIBSDIR} ${LDFLAGS} $(LIBS)

all: ${TARGET}

clean:
	rm -rf ${OBJS} ${TARGET}

install: ${TARGET}
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	$(Q)install -m 0775 ${TARGET} \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
	$(Q)install -m 0664 sample_confg.json \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/
