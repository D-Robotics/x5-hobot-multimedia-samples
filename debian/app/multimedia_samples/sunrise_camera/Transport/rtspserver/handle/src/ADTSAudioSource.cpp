#include "GroupsockHelper.hh"
#include "ADTSAudioSource.hh"
#include "utils/utils_log.h"
#include "utils/stream_define.h"
#include "utils/stream_manager.h"

static unsigned const samplingFrequencyTable[16] =
{
	96000, 88200, 64000, 48000,
	44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000,
	7350, 0, 0, 0
};

ADTSAudioSource* ADTSAudioSource::createNew(UsageEnvironment& env,
	u_int8_t profile,
	u_int8_t samplingFrequencyIndex,
	u_int8_t channelConfiguration)
{
	ADTSAudioSource* source = new ADTSAudioSource(env, profile, samplingFrequencyIndex, channelConfiguration);
	return source;
}
ADTSAudioSource::ADTSAudioSource(UsageEnvironment& env,
	u_int8_t profile,
	u_int8_t samplingFrequencyIndex,
	u_int8_t channelConfiguration)
	: FramedSource(env)
{
	fPresentationTime.tv_sec = 0;
	fPresentationTime.tv_usec = 0;

	fSamplingFrequency = samplingFrequencyTable[samplingFrequencyIndex];
	fNumChannels = channelConfiguration == 0 ? 2 : channelConfiguration;
	fuSecsPerFrame
		= (1024/*samples-per-frame*/ * 1000000) / fSamplingFrequency/*samples-per-second*/;

	// Construct the 'AudioSpecificConfig', and from it, the corresponding ASCII string:
	unsigned char audioSpecificConfig[2];
	u_int8_t const audioObjectType = profile + 1;
	audioSpecificConfig[0] = (audioObjectType << 3) | (samplingFrequencyIndex >> 1);
	audioSpecificConfig[1] = (samplingFrequencyIndex << 7) | (channelConfiguration << 3);
	sprintf(fConfigStr, "%02X%02x", audioSpecificConfig[0], audioSpecificConfig[1]);

	fShmSource = shm_stream_create(AUDIO_MAIN, AUDIO_MAIN, STREAM_MAX_USER, STREAM_AUDIO_MAX_FRAMES, STREAM_AUDIO_MAX_SIZE, SHM_STREAM_READ, SHM_STREAM_MALLOC);

	fPts = -1;
}

ADTSAudioSource::~ADTSAudioSource()
{
	if (fShmSource != NULL) {
		shm_stream_destory(fShmSource);
		fShmSource = NULL;
	}
}


void ADTSAudioSource::doGetNextFrame() {
	//do read from memory
	incomingDataHandler(this);
}

void ADTSAudioSource::incomingDataHandler(ADTSAudioSource* source) {
	if (!source->isCurrentlyAwaitingData())
	{
		source->doStopGettingFrames(); // we're not ready for the data yet
		return;
	}
	source->incomingDataHandler1();
}

void ADTSAudioSource::incomingDataHandler1()
{
	fFrameSize = 0;
	frame_info info;
	unsigned int length;
	unsigned char* data = NULL;

	if(shm_stream_front(fShmSource, &info, &data, &length) == 0)
	{
		fFrameSize = length - 7;//skip adts header
		if (fFrameSize > fMaxSize)
		{
			fNumTruncatedBytes = fFrameSize - fMaxSize;
			fFrameSize = fMaxSize;
		}
		else
		{
			fNumTruncatedBytes = 0;
		}

		unsigned char *pAudioData = (unsigned char*)data + 7;//skip adts header
		memcpy(fTo, pAudioData, fFrameSize);

		if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0)
		{
			// This is the first frame, so use the current time:
			gettickcount(&fPresentationTime, NULL);
			fPts = info.pts / 1000;
		}
		else
		{
			// Increment by the play time of the previous frame:
			unsigned uSeconds = fPresentationTime.tv_usec + (info.pts / 1000 - fPts) * 1000;
			fPresentationTime.tv_sec += uSeconds / 1000000;
			fPresentationTime.tv_usec = uSeconds % 1000000;
			fPts = info.pts / 1000;
		}
		SC_LOGT("framer audio pData length:%d gs_timestamp:%llu fFrameSize:%d", length, info.pts, fFrameSize);
//		fDurationInMicroseconds = fuSecsPerFrame;
		fDurationInMicroseconds = 1000000/200;

		shm_stream_post(fShmSource);

		nextTask() = envir().taskScheduler().scheduleDelayedTask(0, (TaskFunc*)FramedSource::afterGetting, this);
	}
	else
	{
		nextTask() = envir().taskScheduler().scheduleDelayedTask(5 * 1000,
			(TaskFunc*)incomingDataHandler, this);
	}
}

