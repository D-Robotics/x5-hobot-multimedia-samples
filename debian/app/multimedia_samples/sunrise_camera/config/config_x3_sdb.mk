
GLOBAL_INSTALL_DIR := $(PRO_ROOT)sunrise_camera
COMPILE_PREFIX := /opt/gcc-ubuntu-9.3.0-2020.03-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-
export LD_LIBRARY_PATH=/opt/gcc-ubuntu-9.3.0-2020.03-x86_64-aarch64-linux-gnu/lib/x86_64-linux-gnu
CFLAGS_EX  := -Wall -O3 -fstack-protector

APPSDK_DIR ?= $(PRO_ROOT)../../appsdk
dir_test = $(shell if [ -d $(APPSDK_DIR) ]; then echo "exist"; else echo "noexist"; fi)
ifeq ("$(dir_test)", "noexist")
	APPSDK_DIR = $(PRO_ROOT)../../deploy/appsdk

	dir_test = $(shell if [ -d $(APPSDK_DIR) ]; then echo "exist"; else echo "noexist"; fi)
	ifeq ("$(dir_test)", "noexist")
		APPSDK_DIR = $(PRO_ROOT)../../deploy_ubuntu/appsdk
	endif
endif

APPSDK_LIB ?= $(APPSDK_DIR)/lib
APPSDK_INC ?= $(APPSDK_DIR)/include

CHIP_ID ?= CHIP_X3_SDB
############################################################
# MODULE_SYSTEM := y
MODULE_CAMERA := y
# MODULE_NETWORK := y
# MODULE_RECORD := y
# MODULE_ALARM := y
MODULE_RTSP := y
MODULE_WEBSOCKET := y

subdir :=
subdir += common
subdir += communicate
# subdir += Transport
# subdir += Record

ifeq ($(MODULE_SYSTEM), y)
	CFLAGS_EX += -DMODULE_SYSTEM
	subdir += System
endif
ifeq ($(MODULE_CAMERA), y)
	CFLAGS_EX += -DMODULE_CAMERA
	subdir += Platform/$(PLATFORM)
endif
ifeq ($(MODULE_NETWORK), y)
	CFLAGS_EX += -DMODULE_NETWORK
	subdir += Network
endif
ifeq ($(MODULE_RECORD), y)
	CFLAGS_EX += -DMODULE_RECORD
endif
ifeq ($(MODULE_ALARM), y)
	CFLAGS_EX += -DMODULE_ALARM
	subdir += Alarm
endif
ifeq ($(MODULE_RTSP), y)
	CFLAGS_EX += -DMODULE_RTSP
	subdir += Transport/rtspserver/live555
	subdir += Transport/rtspserver
endif
ifeq ($(MODULE_WEBSOCKET), y)
	CFLAGS_EX += -DMODULE_WEBSOCKET
	subdir += Transport/websocket
endif
subdir += main

############################################################
ifeq ($(MODULE_CAMERA), y)
	PLATFORM_LIBS_NAME := hbmedia vio gdcbin iar cam cjson diag isp alog multimedia tinyalsa ion hbmem guvc turbojpeg avcodec avformat avutil z dnn cnn_intf hbrt_bernoulli_aarch64
	PLATFORM_LIBS += $(patsubst %,-l%,$(PLATFORM_LIBS_NAME))
	LDFLAGS_EX += -L$(APPSDK_DIR)/lib/hbmedia
	LDFLAGS_EX += -L$(APPSDK_DIR)/lib/hbbpu
	LDFLAGS_EX += -L$(APPSDK_DIR)/lib/ffmpeg
	LDFLAGS_EX += -L$(APPSDK_DIR)/usr/lib
endif

GLOBAL_EXTERN_INC_DIR += $(APPSDK_DIR)
