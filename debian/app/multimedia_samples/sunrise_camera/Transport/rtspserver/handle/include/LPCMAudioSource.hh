#ifndef _LPCM_AUDIO_SOURCE_HH_
#define _LPCM_AUDIO_SOURCE_HH_

#include "FramedSource.hh"
#include "utils/stream_manager.h"

class LPCMAudioSource : public FramedSource {
public:
	static LPCMAudioSource* createNew(UsageEnvironment& env, u_int8_t bitsPerSample, u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration);
	// "preferredFrameSize" == 0 means 'no preference'
	// "playTimePerFrame" is in microseconds

	unsigned samplingFrequency() const { return fSamplingFrequency; }
	unsigned numChannels() const { return fNumChannels; }
	unsigned bitsPerSample() const { return fBitsPerSample; }
	void sync();
	// returns the 'AudioSpecificConfig' for this stream (in ASCII form)
protected:
	LPCMAudioSource(UsageEnvironment& env, u_int8_t bitsPerSample, u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration);
	// called only by createNew()

	virtual ~LPCMAudioSource();

	static void incomingDataHandler(LPCMAudioSource* source);
	void incomingDataHandler1();

private:
	// redefined virtual functions:
	virtual void doGetNextFrame();
	
private:
	unsigned fBitsPerSample;
	unsigned fSamplingFrequency;
	unsigned fNumChannels;
	unsigned long long	fPts;
	shm_stream_t* 	fShmSource;
};


#endif
