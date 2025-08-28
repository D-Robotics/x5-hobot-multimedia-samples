include ../Makefile.in
CUR_DIR := $(shell pwd)
RELATIVE_PATH := $(shell realpath --relative-to=$(PLATFORM_SAMPLES_DIR) $(CUR_DIR))
$(info RELATIVE_PATH is: $(RELATIVE_PATH))
$(info PLATFORM_SAMPLES_DEPLOY_DIR is: $(PLATFORM_SAMPLES_DEPLOY_DIR))

$(info HR_TARGET_BUILD_DIR is: $(HR_TARGET_BUILD_DIR))
$(info HR_TARGET_PRODUCT_DIR is: $(HR_TARGET_PRODUCT_DIR))
$(info HR_TARGET_DEPLOY_DIR is: $(HR_TARGET_DEPLOY_DIR))

.PHONY: all build install clean

# 设置目标文件名
TARGET = sysinfopro

# 查找所有的 C 源文件
SRCS := $(foreach cf, $(CUR_DIR), $(wildcard $(cf)/*.c))
OBJS :=  $(SRCS:.c=.o)

LIBS += -lcrypto -lhbmem -lalog -lcam -lvpf -ldsp -lboardinfo -lefuse -lhbmem \
		-lhbipcfhal -lalog -lmultimedia -lte600_engine -lz -lminizip -lota_common \
		-lupdate -lpowerctl -lsecure_storage -lteec -lgpiod -ljsoncpp -lkcapi \
		-levent_pthreads -levent -lhbplayer -lNano2D -lebase -loslayer -lversion -lcommon -lisc \
		-lcamlog -lhal -lGAL -lEGL -ldrm_vivante -ldrm -lgbm -lgbm_viv -lcnn_intf -lhbrt_bayes_aarch64 -ldnn

INCS += -I ${HR_BUILD_OUTPUT_DIR}/deploy/system/usr/include/
INCS += -I ${HR_BUILD_OUTPUT_DIR}/deploy/system/usr/include/dnn
INCS += -I ${HR_BUILD_OUTPUT_DIR}/build/hbre_deps/include/GC820/
INCS += -I ${HR_BUILD_OUTPUT_DIR}/build/hbre_deps/include/HAL/
INCS += -I ${HR_BUILD_OUTPUT_DIR}/build/hbre/x5_camsys/build/generated/release/include/hal/

LIBSDIR += -L ${HR_BUILD_OUTPUT_DIR}/deploy/hbre/lib
LIBSDIR += -L ${HR_BUILD_OUTPUT_DIR}/deploy/system/usr/lib

$(info GETINCS is: $(INCS))

# 编译 .c 文件为 .o 文件
%.o:%.c
	@mkdir -p $(abspath $(dir $@))
	$(CC) -o $@ $(INCS) $(CFLAGS) -c $<

# 生成可执行文件
$(TARGET):$(OBJS)
	@mkdir -p $(abspath $(dir $@))
	$(CC) -O0 -o $@ $(OBJS) $(CFLAGS) ${INCS} ${LIBSDIR} $(LIBS)

all: ${TARGET}

# 清理目标
clean:
	rm -rf ${OBJS} ${TARGET}

# 安装目标
install: ${TARGET}
	$(Q)install -d ${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}
	$(Q)install -m 0775 ${TARGET} \
		${PLATFORM_SAMPLES_DEPLOY_DIR}/${RELATIVE_PATH}/

