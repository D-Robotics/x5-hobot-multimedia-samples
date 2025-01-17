#ifndef _UAC_GADGET_H_
#define _UAC_GADGET_H_

#include "uac_common.h"
#include "uac_micphone.h"
#include "uac_speaker.h"

#define UAC_SPEAKER_MASK 		0x1
#define UAC_MICPHONE_MASK 		0x2

typedef struct control_info {
    int mute;
    long volume;
}control_info_t;

typedef struct uac_control {
    alsa_mixer_t mixer;
    pthread_t thread_id;
    int exit;
    control_info_t capture;
    control_info_t playback;
}uac_control_t;

typedef struct uac_gadget_contex_s
{
	uint32_t uac_mask;
	uac_micphone_device_t *uac_micphone;
	uac_speaker_device_t *uac_speaker;
    int speaker_test_type;
    int micphone_test_type;
    char uac_play_file[UAC_FILE_PATH_LEN];
    char uac_record_file[UAC_FILE_PATH_LEN];
    uac_control_t controller;
} uac_gadget_contex_t;

int uac_gadget_contex_init(uac_gadget_contex_t *p_uac_gadget_contex);
int uac_gadget_destroy_and_stop(uac_gadget_contex_t *p_uac_gadget_contex);
void uac_stream_on_or_off(uac_gadget_contex_t *p_uac_gadget_contex, int is_on);

#endif   /* _UAC_GADGET_H_ */
