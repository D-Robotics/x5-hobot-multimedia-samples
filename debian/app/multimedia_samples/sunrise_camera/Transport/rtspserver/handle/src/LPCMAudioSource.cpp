#include "GroupsockHelper.hh"
#include "LPCMAudioSource.hh"
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

LPCMAudioSource* LPCMAudioSource::createNew(UsageEnvironment& env, 
	u_int8_t bitsPerSample, 
	u_int8_t samplingFrequencyIndex, 
	u_int8_t channelConfiguration)
{
	LPCMAudioSource* source = new LPCMAudioSource(env, bitsPerSample, samplingFrequencyIndex, channelConfiguration);
	return source;
}

LPCMAudioSource::LPCMAudioSource(UsageEnvironment& env,
	u_int8_t bitsPerSample, 
	u_int8_t samplingFrequencyIndex, 
	u_int8_t channelConfiguration)
	: FramedSource(env)
{
	fPresentationTime.tv_sec = 0;
	fPresentationTime.tv_usec = 0;

	fBitsPerSample = bitsPerSample;
	fSamplingFrequency = samplingFrequencyTable[samplingFrequencyIndex];
	fNumChannels = channelConfiguration == 0 ? 2 : channelConfiguration;

	char shn_name[64] = {0};
	sprintf(shn_name, "audio%p", this);
	fShmSource = shm_stream_create(shn_name, AUDIO_MAIN, STREAM_MAX_USER, STREAM_AUDIO_MAX_FRAMES, STREAM_AUDIO_MAX_SIZE, SHM_STREAM_READ, SHM_STREAM_MALLOC);

	fPts = 0;
}

void LPCMAudioSource::sync()
{
	shm_stream_sync(fShmSource);
}

LPCMAudioSource::~LPCMAudioSource()
{
	if(fShmSource != NULL)
	{
		shm_stream_destory(fShmSource);
		fShmSource = NULL;
	}
}


void LPCMAudioSource::doGetNextFrame() {
	//do read from memory
	incomingDataHandler(this);
}

void LPCMAudioSource::incomingDataHandler(LPCMAudioSource* source) {
	
	if (!source->isCurrentlyAwaitingData())
	{
		source->doStopGettingFrames(); // we're not ready for the data yet
		return;
	}
	
	source->incomingDataHandler1();
}

void LPCMAudioSource::incomingDataHandler1()
{
	fFrameSize = 0;
	frame_info info;
	unsigned int length;
	unsigned char* data = NULL;
	
	if(shm_stream_get(fShmSource, &info, &data, &length) == 0)
	{
//		printf("framer audio pts:%llu remains:%d length:%d \n", info.pts, shm_stream_remains(fShmSource), length);
		fFrameSize = length;
		if (fFrameSize > fMaxSize)
		{
			fNumTruncatedBytes = fFrameSize - fMaxSize;
			fFrameSize = fMaxSize;
		}
		else
		{
			fNumTruncatedBytes = 0;
		}

		memcpy(fTo, data, fFrameSize);

		if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0)
		{
			// This is the first frame, so use the current time:
			gettickcount(&fPresentationTime, NULL);
			fPts = info.pts;
		}
		else
		{
			// Increment by the play time of the previous frame:
			unsigned uSeconds = fPresentationTime.tv_usec + (info.pts - fPts);
			fPresentationTime.tv_sec += uSeconds / 1000000;
			fPresentationTime.tv_usec = uSeconds % 1000000;
			fPts = info.pts;
		}
		
		int remains = shm_stream_remains(fShmSource);
		if(remains > 5)
			SC_LOGI("framer audio pts:%llu length:%d fFrameSize:%d remains:%d", info.pts, length, fFrameSize, remains);

		unsigned bytesPerSample = (fNumChannels*fBitsPerSample)/8;
		double fPlayTimePerSample = 1e6/(double)fSamplingFrequency;
		fDurationInMicroseconds = (unsigned)((fPlayTimePerSample*fFrameSize)/bytesPerSample);

		FramedSource::afterGetting(this);
//		nextTask() = envir().taskScheduler().scheduleDelayedTask(5, (TaskFunc*)FramedSource::afterGetting, this);
	}
	else
	{
		nextTask() = envir().taskScheduler().scheduleDelayedTask(500,
			(TaskFunc*)incomingDataHandler, this);
	}
}

