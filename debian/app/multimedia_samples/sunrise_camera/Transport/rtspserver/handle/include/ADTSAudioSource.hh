#ifndef _ADTS_AUDIO_SOURCE_HH_
#define _ADTS_AUDIO_SOURCE_HH_

#include "FramedSource.hh"
#include "utils/stream_manager.h"

class ADTSAudioSource : public FramedSource {
public:
	static ADTSAudioSource* createNew(UsageEnvironment& env, u_int8_t profile, u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration);
	// "preferredFrameSize" == 0 means 'no preference'
	// "playTimePerFrame" is in microseconds

	unsigned samplingFrequency() const { return fSamplingFrequency; }
	unsigned numChannels() const { return fNumChannels; }
	char const* configStr() const { return fConfigStr; }
	// returns the 'AudioSpecificConfig' for this stream (in ASCII form)
protected:
	ADTSAudioSource(UsageEnvironment& env, u_int8_t profile, u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration);
	// called only by createNew()

	virtual ~ADTSAudioSource();

	static void incomingDataHandler(ADTSAudioSource* source);
	void incomingDataHandler1();

private:
	// redefined virtual functions:
	virtual void doGetNextFrame();
	
private:
	unsigned fSamplingFrequency;
	unsigned fNumChannels;
	unsigned fuSecsPerFrame;
	char 	fConfigStr[5];
	unsigned int	fPts;
	shm_stream_t* 	fShmSource;
};


#endif
