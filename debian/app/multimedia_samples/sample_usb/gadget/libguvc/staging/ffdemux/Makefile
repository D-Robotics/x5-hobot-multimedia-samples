include ./Makefile.in

ifeq (${BUILD_OUTPUT_PATH},)
export BUILD_OUTPUT_PATH=./
endif

.PHONY: all all_32 clean install dist-clean

SRC = $(wildcard *.c)

OBJECT := ${BUILD_OUTPUT_PATH}/ffdemux
OBJS := $(addprefix ${BUILD_OUTPUT_PATH}/,$(SRC:.c=.o))

CFLAGS +=
LDFLAGS += -L./ -L${BUILD_OUTPUT_PATH} -L${DEPLOY_SYSTEM_DIR}/usr/lib

INCS += -I./inc -I${DEPLOY_SYSTEM_DIR}/usr/include
LIBS += -lpthread -lavformat -lavcodec -lavutil -lz -lssl -lcrypto -ldrm

all all_32: $(OBJECT)

$(OBJECT): $(OBJS)
	@mkdir -p $(abspath $(dir $@))
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@

${BUILD_OUTPUT_PATH}/%.o: %.c
	@mkdir -p $(abspath $(dir $@))
	$(CC) $(CFLAGS) -c $< $(INCS) -o $@

install:
	@echo "do nothing for ffdemux install"

dist-clean:
	@echo "do nothing for ffdemux dist-clean"

clean:
	rm -rf $(OBJECT) $(OBJS)
