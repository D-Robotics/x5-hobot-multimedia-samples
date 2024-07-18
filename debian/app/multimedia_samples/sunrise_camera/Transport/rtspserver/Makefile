include ./makefile.param

INC_DIR := api handle main
INC_DIR += $(GLOBAL_EXTERN_INC_DIR) $(GLOBAL_INSTALL_DIR)
INC := $(patsubst %,-I%/include,$(INC_DIR))
LIB := $(patsubst %,-L%/lib,$(INC_DIR))

# 添加live555库的头文件引用
LIVE555_INC_DIR := BasicUsageEnvironment groupsock liveMedia UsageEnvironment
LIVE555_INC := $(patsubst %,-Ilive555/%/include,$(LIVE555_INC_DIR))

SRC := $(wildcard *.cpp *.c $(patsubst %,%/src/*.cpp,$(INC_DIR)) $(patsubst %,%/src/*.c,$(INC_DIR)))
OBJ := $(patsubst %.cpp,%.obj,$(patsubst %.c,%.o,$(SRC)))
DEP := $(patsubst %.obj,%.dep,$(patsubst %.o,%.d,$(OBJ)))

CC := $(COMPILE_PREFIX)gcc
CFLAGS := $(INC) $(LIVE555_INC) -O0 -DBSD=1 -fPIC $(CFLAGS_EX)
#CFLAGS := $(INC) -O0 -Wall -DBSD=1 -fPIC
CXX := $(COMPILE_PREFIX)g++
CXXFLAGS := $(CFLAGS)
LINK := $(COMPILE_PREFIX)g++ -o
LIBRARY_LINK := $(COMPILE_PREFIX)g++ -shared -o
LIBRARY_LINK_STATIC := $(COMPILE_PREFIX)ar cr
LIBRARY_LINK_OPTS =
STRIP := $(COMPILE_PREFIX)strip

TARGET := librtspserver.a

.PHONY : all clean

all : $(TARGET) install

%.o : %.c
	$(CC) $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -c $< -o $@

%.obj : %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -MF"$(@:%.obj=%.dep)" -MT"$(@:%.obj=%.dep)" -c $< -o $@

$(TARGET) : $(OBJ)
	$(LIBRARY_LINK_STATIC) $@ $^

install:
	-mkdir -p $(GLOBAL_INSTALL_DIR)/lib
	mv $(TARGET) $(GLOBAL_INSTALL_DIR)/lib

clean:
	@rm -rf $(OBJ) $(DEP) $(TARGET)

sinclude $(DEP)

