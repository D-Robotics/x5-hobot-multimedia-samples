#ifndef ___UVC_GADGET_WRAPER_HH__
#define ___UVC_GADGET_WRAPER_HH__

#include "uvc_gadget_api.h"


typedef struct uvc_gadget_camera_info_s
{
    int frame_width;
    int frame_height;

    struct uvc_prepare_callback prepare_frame_cb;
    struct uvc_release_callback release_frame_cb;
    struct uvc_streamon_callback stream_on_or_off_cb;

}uvc_gadget_camera_info_t;

struct uvc_context *uvc_gadget_create_and_start(uvc_gadget_camera_info_t *uvc_gadget_camera_info);
void uvc_gadget_destroy_and_stop(struct uvc_context *ctx);

#endif //