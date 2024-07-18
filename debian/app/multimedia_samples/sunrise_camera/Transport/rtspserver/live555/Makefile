include ./makefile.param

INC_DIR := BasicUsageEnvironment groupsock liveMedia UsageEnvironment
#INC_DIR += $(GLOBAL_EXTERN_INC_DIR) $(GLOBAL_INSTALL_DIR)
INC := $(patsubst %,-I%/include,$(INC_DIR))
#LIB := $(patsubst %,-L%/lib,$(INC_DIR))

SRC := $(wildcard *.cpp *.c $(patsubst %,%/*.cpp,$(INC_DIR)) $(patsubst %,%/*.c,$(INC_DIR)))
OBJ := $(patsubst %.cpp,%.obj,$(patsubst %.c,%.o,$(SRC)))
DEP := $(patsubst %.obj,%.dep,$(patsubst %.o,%.d,$(OBJ)))

#$(warning $(INC_DIR) $(INC) $(SRC))

CC := $(COMPILE_PREFIX)gcc
# 添加 DSOCKLEN_T=socklen_t 解决以下error:
#	“groupsock/GroupsockHelper.cpp:430:21:
#	error: invalid conversion from ‘int*’ to ‘socklen_t* {aka unsigned int*}’
#	[-fpermissive]”
CFLAGS := $(INC) -O0 -DBSD=1 -fPIC $(CFLAGS_EX) -DSOCKLEN_T=socklen_t -DALLOW_SERVER_PORT_REUSE
#CFLAGS := $(INC) -O0 -Wall -DBSD=1 -fPIC
CXX := $(COMPILE_PREFIX)g++
CXXFLAGS := $(CFLAGS)
LINK := $(COMPILE_PREFIX)g++ -o
LIBRARY_LINK := $(COMPILE_PREFIX)g++ -shared -o
LIBRARY_LINK_STATIC := $(COMPILE_PREFIX)ar cr
LIBRARY_LINK_OPTS =
STRIP := $(COMPILE_PREFIX)strip

TARGET := liblive555.a

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

