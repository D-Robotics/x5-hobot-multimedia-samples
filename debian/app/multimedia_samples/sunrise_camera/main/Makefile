include ../makefile.param

INC_DIR := .
INC_DIR += $(GLOBAL_EXTERN_INC_DIR) $(GLOBAL_INSTALL_DIR)
INC := $(patsubst %,-I%/include,$(INC_DIR))
LIB := $(patsubst %,-L%/lib,$(INC_DIR))
BASIC_LIB  := -lcommunicate
MODULE_LIB :=
UTILS_LIB  := -lutils

SRC := $(wildcard *.cpp *.c $(patsubst %,%/src/*.cpp,$(INC_DIR)) $(patsubst %,%/src/*.c,$(INC_DIR)))
OBJ := $(patsubst %.cpp,%.obj,$(patsubst %.c,%.o,$(SRC)))
DEP := $(patsubst %.obj,%.dep,$(patsubst %.o,%.d,$(OBJ)))

ifeq ($(MODULE_SYSTEM), y)
	MODULE_LIB += -lsystem
endif
ifeq ($(MODULE_VPP), y)
	MODULE_LIB += -lvpp
endif
ifeq ($(MODULE_NETWORK), y)
	MODULE_LIB += -lnetwork
endif
ifeq ($(MODULE_RECORD), y)
	MODULE_LIB += -lrecord
endif
ifeq ($(MODULE_ALIYUN), y)
	MODULE_LIB += -laliyun -lapr-1 -laprutil-1 -lexpat -lcurl -lmxml
endif
ifeq ($(MODULE_FACTURE), y)
	MODULE_LIB += -lmanufacture
endif
ifeq ($(MODULE_UP2P), y)
	MODULE_LIB += -lup2p -lp2p
endif
ifeq ($(MODULE_ULIFE), y)
	MODULE_LIB += -lulife
endif
ifeq ($(MODULE_ALARM), y)
	MODULE_LIB += -lalarm
endif
ifeq ($(MODULE_RTMP), y)
	MODULE_LIB += -lrtmppusher -lrtmp
endif
ifeq ($(MODULE_RTSP), y)
	MODULE_LIB += -lrtspserver -llive555 -lstdc++
endif
ifeq ($(MODULE_WEBSOCKET), y)
	MODULE_LIB += -lwebsocket
endif

CC := $(COMPILE_PREFIX)gcc
CFLAGS := $(INC) -Werror -O0 -DBSD=1 -fPIC $(CFLAGS_EX)
#CFLAGS := $(INC) -O0 -Wall -DBSD=1 -fPIC
CXX := $(COMPILE_PREFIX)g++
CXXFLAGS := $(CFLAGS)
LINK := $(COMPILE_PREFIX)gcc -o
LIBRARY_LINK := $(COMPILE_PREFIX)g++ -shared -o
LIBRARY_LINK_STATIC := $(COMPILE_PREFIX)ar cr
LIBRARY_LINK_OPTS =	$(LDFLAGS_EX) $(BASIC_LIB) $(MODULE_LIB) $(UTILS_LIB) $(PLATFORM_LIBS) -lpthread -lm -ldl -lrt $(LIB)
STRIP := $(COMPILE_PREFIX)strip

TARGET := sunrise_camera

.PHONY : all clean

all : $(TARGET) install

%.o : %.c
	$(CC) $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -c $< -o $@

%.obj : %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -MF"$(@:%.obj=%.dep)" -MT"$(@:%.obj=%.dep)" -c $< -o $@

$(TARGET) : $(OBJ)
	$(LINK) $@ $^ $(LIBRARY_LINK_OPTS)
install:
	-mkdir -p $(GLOBAL_INSTALL_DIR)/bin
	#$(STRIP) $(TARGET)
	mv $(TARGET) $(GLOBAL_INSTALL_DIR)/bin
clean:
	@rm -rf $(OBJ) $(DEP) $(TARGET)

sinclude $(DEP)
