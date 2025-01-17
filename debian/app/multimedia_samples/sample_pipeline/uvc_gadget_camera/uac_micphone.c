#include <stdio.h>
#include "uac_micphone.h"
#include <pthread.h>
#include "utils.h"

static void *uac_micphone_loop(void *arg)
{
	uac_micphone_device_t *dev = (uac_micphone_device_t *)arg;
	snd_pcm_sframes_t frames;
	char *buffer;
	int r, size;
	FILE *fd;

	trace_in();

	if (!dev || !dev->uac_device) {
		fprintf(stderr, "%s-%d error happen\n", __func__, __LINE__);
		return (void *)0;
	}

	fd = fopen(dev->play_file, "rb");
	if (fd < 0) {
		fprintf(stderr, "uac micphone test thread: open %s failed\n", dev->play_file);
		return (void *)0;
	}

	alsa_device_t *uac_dev = dev->uac_device;

	// frames = uac_dev->period_size;
	frames = uac_dev->period_size;
	size = snd_pcm_frames_to_bytes(uac_dev->handle, frames);
	buffer = malloc(size);

	printf("%s, frames(%d), size(%d)\n", __func__, (int)frames, size);

	/* audio process loop */
	while (!dev->exit) {
		fseek(fd, 0, SEEK_SET);
		while ((fread(buffer, 1, size, fd)) > 0 || !feof(fd)) {
			if (dev->stream_on)
			{
				/* write data to uac playback device */
				r = alsa_device_write(uac_dev, buffer, frames);
				if (r < 0) {
					printf("UAC alsa_device_write error[%d]\n", r);
				}
			}
			if (dev->exit) {
				break;
			}
		}
	}

	if (buffer)
		free(buffer);
	if (fd)
		fclose(fd);

	trace_out();

	return (void *)0;
}

int uac_micphone_init(uac_micphone_device_t **uac_micphone)
{
	int r;
	alsa_device_t *uac_device = NULL;

	trace_in();

	uac_micphone_device_t *dev = (uac_micphone_device_t *)calloc(1,
			sizeof(uac_micphone_device_t));
	if (!dev) {
		r = -ENOMEM;
		goto err1;
	}

	uac_device = alsa_device_allocate();
	if (!uac_device) {
		r = -ENOMEM;
		goto err2;
	}

	/* init uac recorder device*/
	uac_device->name	= "hw:1,0";   // uac_playback
	uac_device->format	= SND_PCM_FORMAT_S16;
	uac_device->direct	= SND_PCM_STREAM_PLAYBACK;
	uac_device->rate	= 48000;
	uac_device->channels	= 2;
	uac_device->buffer_time = 0;		// use default buffer time
	uac_device->nperiods	= 4;
	// uac_device->period_size = 1024;		// 1 period including 1024 frames

	r = alsa_device_init(uac_device);
	if (r < 0) {
		fprintf(stderr, "alsa_device_init %s failed. r(%d)\n",
				uac_device->name, r);
		goto err2;
	}

	dev->uac_device = uac_device;
	dev->thread_id = 0;
	dev->exit = 0;
	*uac_micphone = dev;

	trace_out();
	return 0;

err2:
	if (uac_device)
		free(uac_device);
err1:
	return r;
}

int uac_micphone_start(uac_micphone_device_t *uac_micphone)
{
	int r;

	trace_in();

	if (!uac_micphone)
		return -EINVAL;

	uac_micphone->exit = 0;

	r = pthread_create(&uac_micphone->thread_id, NULL, uac_micphone_loop,uac_micphone);
	if (r < 0)
		fprintf(stderr, "create thread uvc_micphone_loop failed\n");

	trace_out();

	return r;
}

int uac_micphone_stop(uac_micphone_device_t *uac_micphone)
{
	int r;

	trace_in();

	if (!uac_micphone)
		return -EINVAL;

	/* force uac micphone stop */
	uac_micphone->exit = 1;

	r = pthread_join(uac_micphone->thread_id, NULL);
	if (r < 0)
		fprintf(stderr, "uvc_micphone thread join failed\n");

	trace_out();

	return r;
}

void uac_micphone_deinit(uac_micphone_device_t *uac_micphone)
{
	trace_in();

	if (!uac_micphone)
		return;

	if (uac_micphone->micphone_device) {
		alsa_device_deinit(uac_micphone->micphone_device);
		alsa_device_free(uac_micphone->micphone_device);
	}

	if (uac_micphone->uac_device) {
		alsa_device_deinit(uac_micphone->uac_device);
		alsa_device_free(uac_micphone->uac_device);
	}

	free(uac_micphone);

	trace_out();
}
