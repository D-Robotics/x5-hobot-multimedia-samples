/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * uac_speaker.h
 *	uac speaker api.
 *
 * Copyright (C) 2024 D-Robotics, Inc.
 *
 * Contact: jianghe xu<jianghe.xu@d-robotics.cc>
 */

#ifndef _UAC_SPEAKER_H_
#define _UAC_SPEAKER_H_

#include "uac_common.h"
#include "alsa_device.h"

typedef struct uac_speaker_device {
	alsa_device_t *uac_device;
	alsa_device_t *speaker_device;
	pthread_t thread_id;
	int exit;
	int stream_on;
	int test_type;
	char record_file[UAC_FILE_PATH_LEN];
} uac_speaker_device_t;

uac_speaker_device_t *uac_speaker_allocate(void);

int uac_speaker_init(uac_speaker_device_t **uac_speaker);
int uac_speaker_start(uac_speaker_device_t *uac_speaker);
int uac_speaker_stop(uac_speaker_device_t *uac_speaker);
void uac_speaker_deinit(uac_speaker_device_t *uac_speaker);

#endif	/* _UAC_SPEAKER_H_ */
