include ./Makefile.in

ifeq (${BUILD_OUTPUT_PATH},)
export BUILD_OUTPUT_PATH=./
endif

$(info BUILD_OUTPUT_PATH directory (absolute): $(BUILD_OUTPUT_PATH))

CFLAGS  += -D_GNU_SOURCE
LDFLAGS += -L./ -L${BUILD_OUTPUT_PATH} -L${DEPLOY_SYSTEM_DIR}/usr/lib
LIBS += -lpthread -lavformat -lavcodec -lavutil -lguvc -lz -lssl -lcrypto -ldrm
LIBS_STATIC = -Bstatic $(filter-out -lguvc, $(LIBS))
INCS += -I./inc -I${DEPLOY_SYSTEM_DIR}/usr/include

BINARY = ${BUILD_OUTPUT_PATH}/uvc-gadget
BINARY2 = ${BUILD_OUTPUT_PATH}/uvc-gadget-static
LIB_SO = ${BUILD_OUTPUT_PATH}/libguvc.so
LIB_A = ${BUILD_OUTPUT_PATH}/libguvc.a

SRC = demo/uvc_app.c 	\
	  demo/video_queue.c \
	  demo/uvc_event_handler.c \
	  demo/camera_lib.c
LIB_SRC = src/uvc_gadget.c 	\
	  src/uvc_gadget_api.c

.PHONY: clean all install

OBJS = $(addprefix ${BUILD_OUTPUT_PATH}/,$(SRC:.c=.o))
LIB_OBJS = $(addprefix ${BUILD_OUTPUT_PATH}/,$(LIB_SRC:.c=.o))

all:$(LIB_SO) $(LIB_A) $(BINARY) $(BINARY2)

${BUILD_OUTPUT_PATH}/%.o:%.c
	@mkdir -p $(abspath $(dir $@))
	$(CC) $(CFLAGS) -c $<  $(INCS) -fPIC -o $@

$(LIB_SO):$(LIB_OBJS)
	@mkdir -p $(abspath $(dir $@))
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -shared -o $@

$(LIB_A):$(LIB_OBJS)
	@mkdir -p $(abspath $(dir $@))
	$(AR) $(CFLAGS_STATIC) $@ $^

$(BINARY):$(OBJS) $(LIB_SO)
	@mkdir -p $(abspath $(dir $@))
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

$(BINARY2):$(OBJS) $(LIB_A)
	@mkdir -p $(abspath $(dir $@))
	$(CC) $(CFLAGS) $^  $(LDFLAGS) $(LIBS_STATIC) -o $@

install:
	install -p -D -m 755 $(LIB_SO) ${BUILD_OUTPUT_PATH}/_install/lib/$(LIB_SO)
	install -p -D -m 755 $(BINARY) ${BUILD_OUTPUT_PATH}/_install/bin/$(BINARY)
	cp -rf inc ${BUILD_OUTPUT_PATH}/_install/

	install -p -D -m 755 $(LIB_SO) ${PLATFORM_SAMPLES_DEPLOY_DIR}/sample_usb/libguvc/$(LIB_SO)
	install -p -D -m 755 $(BINARY) ${PLATFORM_SAMPLES_DEPLOY_DIR}/sample_usb/libguvc/$(BINARY)

distclean:
	rm -rf $(BINARY) $(BINARY2) $(OBJS) $(LIB_SO) $(LIB_A) $(LIB_OBJS) ${BUILD_OUTPUT_PATH}/_install

clean:
	rm -rf $(BINARY) $(BINARY2) $(OBJS) $(LIB_SO) $(LIB_A) $(LIB_OBJS)
