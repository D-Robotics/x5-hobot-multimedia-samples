#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

#include "uac_gadget.h"

/** Speaker */
static int launch_uac_speaker(uac_gadget_contex_t *p_uac_gadget_contex)
{
	int ret;
    uac_speaker_device_t *uac_speaker = NULL;

	ret = uac_speaker_init(&p_uac_gadget_contex->uac_speaker);
	if (ret < 0)
		goto launch_uac_speaker_err1;

    uac_speaker = p_uac_gadget_contex->uac_speaker;
    uac_speaker->test_type = E_UAC_TEST_FILE;
    memcpy(uac_speaker->record_file, p_uac_gadget_contex->uac_record_file, UAC_FILE_PATH_LEN);

	ret = uac_speaker_start(p_uac_gadget_contex->uac_speaker);
	if (ret < 0)
		goto launch_uac_speaker_err2;

	return 0;

launch_uac_speaker_err2:
	uac_speaker_deinit(p_uac_gadget_contex->uac_speaker);
	p_uac_gadget_contex->uac_speaker = NULL;

launch_uac_speaker_err1:
	return ret;
}

static void stop_uac_speaker(uac_gadget_contex_t *p_uac_gadget_contex)
{
	if (!p_uac_gadget_contex || !p_uac_gadget_contex->uac_speaker)
		return;

	uac_speaker_stop(p_uac_gadget_contex->uac_speaker);

	uac_speaker_deinit(p_uac_gadget_contex->uac_speaker);
	p_uac_gadget_contex->uac_speaker = NULL;
}

/** Microphone */
static int launch_uac_micphone(uac_gadget_contex_t *p_uac_gadget_contex)
{
	int ret;
    uac_micphone_device_t *uac_micphone = NULL;

	ret = uac_micphone_init(&p_uac_gadget_contex->uac_micphone);
	if (ret < 0)
		goto launch_uac_micphone_err1;

    uac_micphone = p_uac_gadget_contex->uac_micphone;
    uac_micphone->test_type = E_UAC_TEST_FILE;
    memcpy(uac_micphone->play_file, p_uac_gadget_contex->uac_play_file, UAC_FILE_PATH_LEN);

	ret = uac_micphone_start(p_uac_gadget_contex->uac_micphone);
	if (ret < 0)
		goto launch_uac_micphone_err2;

	return 0;

launch_uac_micphone_err2:
	uac_micphone_deinit(p_uac_gadget_contex->uac_micphone);
	p_uac_gadget_contex->uac_micphone = NULL;

launch_uac_micphone_err1:
	return ret;
}

static void stop_uac_micphone(uac_gadget_contex_t *p_uac_gadget_contex)
{
	if (!p_uac_gadget_contex || !p_uac_gadget_contex->uac_micphone)
		return;

	uac_micphone_stop(p_uac_gadget_contex->uac_micphone);

	uac_micphone_deinit(p_uac_gadget_contex->uac_micphone);
	p_uac_gadget_contex->uac_micphone = NULL;
}

void uac_stream_on_or_off(uac_gadget_contex_t *p_uac_gadget_contex, int is_on)
{
	if (!p_uac_gadget_contex) {
		return;
	}

	if (p_uac_gadget_contex->uac_micphone) {
		p_uac_gadget_contex->uac_micphone->stream_on = is_on;
	}

	if (p_uac_gadget_contex->uac_speaker) {
		p_uac_gadget_contex->uac_speaker->stream_on = is_on;
	}
}

static void *uac_controller_loop(void *arg)
{
	uac_control_t *controller = (uac_control_t *)arg;
	control_info_t *capture = &controller->capture;
	control_info_t *playback = &controller->playback;
	long volume = 0;
	int mute = 0;
	while (!controller->exit) {
		if (0 != alsa_mixer_init(&controller->mixer)) {
			printf("alsa_mixer_init error\n");
			break;
		}

		if (0 == alsa_mixer_get_capture_volume_mute(&controller->mixer, &volume, &mute)) {
			if (volume != capture->volume || mute != capture->mute) {
				capture->volume = volume;
				capture->mute = mute;
				printf("Capture Control: Vol[%ld] Mute[%d]\n", capture->volume, capture->mute);
				/* Hardware Control */
				/**/
			}
		}
		if (0 == alsa_mixer_get_playback_volume_mute(&controller->mixer, &volume, &mute)) {
			if (volume != playback->volume || mute != playback->mute) {
				playback->volume = volume;
				playback->mute = mute;
				printf("playback Control: Vol[%ld] Mute[%d]\n", playback->volume, playback->mute);
				/* Hardware Control */
				/**/
			}
		}
		alsa_mixer_deinit(&controller->mixer);
		sleep(2);
	}
	printf("uac_controller_loop Exit.");

	return (void *)0;
}

/** Init */
int uac_gadget_contex_init(uac_gadget_contex_t *p_uac_gadget_contex)
{
	if (!p_uac_gadget_contex) {
		printf("uac contex init fail: empty param\n");
		return -1;
	}

	if (p_uac_gadget_contex->uac_mask & UAC_SPEAKER_MASK) {
		printf("uac add speaker test.\n");
        launch_uac_speaker(p_uac_gadget_contex);
	}
	if (p_uac_gadget_contex->uac_mask & UAC_MICPHONE_MASK) {
		printf("uac add microphone test.\n");
        launch_uac_micphone(p_uac_gadget_contex);
	}

	if (p_uac_gadget_contex->uac_mask) {
		uac_control_t *controller = &p_uac_gadget_contex->controller;
		alsa_mixer_t *mixer = &controller->mixer;
		mixer->device = "hw:1";

		if (0 > pthread_create(&controller->thread_id, NULL, uac_controller_loop, controller)) {
			fprintf(stderr, "create thread uac_controller_loop failed\n");
		}
	}

    return 0;
}

/** Deinit */
int uac_gadget_destroy_and_stop(uac_gadget_contex_t *p_uac_gadget_contex)
{
    if (!p_uac_gadget_contex) {
		return 0;
	}

	if (p_uac_gadget_contex->uac_mask & UAC_SPEAKER_MASK) {
		printf("uac stop speaker test.\n");
        stop_uac_speaker(p_uac_gadget_contex);
	}
	if (p_uac_gadget_contex->uac_mask & UAC_MICPHONE_MASK) {
		printf("uac stop microphone test.\n");
        stop_uac_micphone(p_uac_gadget_contex);
	}

	if (p_uac_gadget_contex->uac_mask) {
		uac_control_t *controller = &p_uac_gadget_contex->controller;
		//alsa_mixer_t *mixer = &controller->mixer;
		controller->exit = 1;
		pthread_join(controller->thread_id, NULL);
	}

    return 0;
}
