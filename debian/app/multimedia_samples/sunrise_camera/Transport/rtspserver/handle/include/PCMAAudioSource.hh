#ifndef _PCMA_AUDIO_SOURCE_HH_
#define _PCMA_AUDIO_SOURCE_HH_

#include "FramedSource.hh"
#include "utils/stream_manager.h"

extern shm_stream_t* 		fPCMAAdoShmSource;
class PCMAAudioSource : public FramedSource {
public:
	static PCMAAudioSource* createNew(UsageEnvironment& env, u_int8_t bitsPerSample, u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration);
	// "preferredFrameSize" == 0 means 'no preference'
	// "playTimePerFrame" is in microseconds

	unsigned samplingFrequency() const { return fSamplingFrequency; }
	unsigned numChannels() const { return fNumChannels; }
	unsigned bitsPerSample() const { return fBitsPerSample; }
	void sync();

	shm_stream_t* 	fShmSource;
	// returns the 'AudioSpecificConfig' for this stream (in ASCII form)
protected:
	PCMAAudioSource(UsageEnvironment& env, u_int8_t bitsPerSample, u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration);
	// called only by createNew()

	virtual ~PCMAAudioSource();

	static void incomingDataHandler(PCMAAudioSource* source);
	void incomingDataHandler1();

private:
	// redefined virtual functions:
	virtual void doGetNextFrame();
	
private:
	unsigned fBitsPerSample;
	unsigned fSamplingFrequency;
	unsigned fNumChannels;
	unsigned long long	fPts;
};


#endif
