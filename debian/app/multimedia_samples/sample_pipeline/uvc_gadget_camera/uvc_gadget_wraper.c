#include "uvc_gadget_wraper.h"

struct uvc_context *uvc_gadget_create_and_start(uvc_gadget_camera_info_t *camera_info){
	struct uvc_context *ctx = NULL;
    struct uvc_params params;
    uvc_gadget_user_params_init(&params);
    params.io_method = 0; //mmap
    params.speed = 3;   // ?
    params.mult = 2;   // ?

	params.format = 1;   //NV12
	params.width = camera_info->frame_width;
	params.height = camera_info->frame_height;

	if (uvc_gadget_init(&ctx, "/dev/video0", NULL, &params)) {
		fprintf(stderr, "uvc_gadget_init failed\n");
		return NULL;
	}

	//callback
    uvc_set_prepare_data_handler(ctx, camera_info->prepare_frame_cb.cb, camera_info->prepare_frame_cb.userdata);
    uvc_set_release_data_handler(ctx, camera_info->release_frame_cb.cb, camera_info->release_frame_cb.userdata);
    uvc_set_streamon_handler(ctx, camera_info->stream_on_or_off_cb.cb, camera_info->stream_on_or_off_cb.userdata);

	if (uvc_gadget_start(ctx) < 0) {
		fprintf(stderr, "uvc_gadget_start failed\n");
		return NULL;
	}
	return ctx;
}

void uvc_gadget_destroy_and_stop(struct uvc_context *ctx){
	if (uvc_gadget_stop(ctx) < 0)
		fprintf(stderr, "uvc_gdaget_stop failed\n");
	uvc_gadget_deinit(ctx);
}