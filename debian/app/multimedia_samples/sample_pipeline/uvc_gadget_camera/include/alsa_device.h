/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * alsa_device.h
 *	alsa device header file
 *
 * Copyright (C) 2024 D-Robotics, Inc.
 *
 * Contact: jianghe xu<jianghe.xu@d-robotics.cc>
 */

#ifndef _ALSA_DEVICE_H_
#define _ALSA_DEVICE_H_

#include <alsa/asoundlib.h>

typedef struct alsa_device {
	snd_pcm_t		    *handle;		/* sound device handle */
	char			    *name;			/* alsa device name (eg. default) */
	snd_pcm_format_t	format;			/* sample format */
	snd_pcm_stream_t	direct;			/* stream direction */
	unsigned int		rate;           /* stream rate */
	unsigned int		channels;       /* count of channels */
	unsigned int		buffer_time;    /* ring buffer length in us */
	unsigned int		period_time;    /* period time in us */
	unsigned int		nperiods;       /* number of periods */
	int			        mode;			/* SND_PCM_NONBLOCK, SND_PCM_ASYNC... */
	snd_pcm_uframes_t	period_size;	/* period_size, how many frames one period contains */
	snd_pcm_uframes_t	buffer_size;	/* buffer_size, totally alsa buffer. nperiods * period_size */
} alsa_device_t;

typedef struct alsa_mixer {
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	char *device;
}alsa_mixer_t;

alsa_device_t *alsa_device_allocate(void);
int alsa_device_init(alsa_device_t *adev);
int alsa_device_read(alsa_device_t *adev, void *buffer,
		snd_pcm_uframes_t frames);
int alsa_device_write(alsa_device_t *adev, void *buffer,
		snd_pcm_uframes_t frames);
void alsa_device_deinit(alsa_device_t *adev);
void alsa_device_free(alsa_device_t *obj);

/* helper function */
void alsa_device_debug_enable(int enable);

/* alsa mixer */
int alsa_mixer_init(alsa_mixer_t *mixer);
void alsa_mixer_deinit(alsa_mixer_t *mixer);
int alsa_mixer_get_capture_volume_mute(alsa_mixer_t *mixer, long *volume, int *mute);
int alsa_mixer_get_playback_volume_mute(alsa_mixer_t *mixer, long *volume, int *mute);

#endif	/* _ALSA_DEVICE_H_ */