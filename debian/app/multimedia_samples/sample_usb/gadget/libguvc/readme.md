## libguvc.so & uvc-gadget
libguvc.so is the uvc-gadget library.
uvc-gadget is a demo application for USB Class Video Gadget.

support formats:
- yuyv
- mjpeg
- h264

to be added
- nv12
- h265

## build
source build/envsetup.sh
make clean
make

## install/release
make install
then release libs & include headers will be in _install folder.

├── _install
│   ├── bin
│   │   └── uvc-gadget
│   ├── inc
│   │   ├── uvc_gadget_api.h
│   │   ├── uvc_gadget.h
│   │   ├── uvc.h
│   │   └── video_queue.h
│   └── lib
│       └── libguvc.so

## how to program
All api in uvc_gadget_api.h, as below:

    uvc_gadget_user_params_init(struct uvc_params *params)
    uvc_gadget_init(struct uvc_context **pctx, char *uvc_devname, char *v4l2_devname, struct uvc_params *user_params)
    uvc_set_streamon_handler(struct uvc_context *ctx, uvc_streamon_callback_fn cb_fn, void *userdata)
    uvc_set_pull_data_handler(struct uvc_context *ctx, uvc_fill_buffer_callback_fn cb_fn, void *userdata)
    uvc_set_prepare_data_handler(struct uvc_context *ctx, uvc_prepare_buffer_callback_fn cb_fn, void *userdata)
    uvc_set_release_data_handler(struct uvc_context *ctx, uvc_release_buffer_callback_fn cb_fn, void *userdata)
    uvc_gadget_start(struct uvc_context *ctx)
    uvc_gadget_stop(struct uvc_context *ctx)
    uvc_gadget_deinit(struct uvc_context *ctx)

uvc.app is the reference uvc app. Please refer to it for the logic.
    Usually, the function sequence like below:

    params_init(some user default params is initialized) -> 
    re-set some special user params ->
    init ->
    set_callback_function (streamon-off, prepare/release buffer)
        streamon-off : control frontend vio/encoder and related thread start/stop
        prepare/release : prepare buffer in prepare callback, release buffer in release callback
    start ->
    .....uvc loop (which will handle control/streaming event!!)
    stop(user quit or exception happen...) -> 
    deinit ->

For structure & function detail!! Please refer to uvc_gadget_api.h!!

## how to run it
uvc:
service adbd stop
modprobe g_webcam streaming_bulk=1
uvc-gadget -u /dev/video8 -i /userdata/1080p.mjepg -r 2 -f 2 -o 0 -b  # 1080p mjpeg
uvc-gadget -u /dev/video8 -i /userdata/ip_1080p.h264-r 2 -f 3 -o 0 -b  # 1080p h264

## that's it.
