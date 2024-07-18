#ifndef _H265_VIDEO_LIVE_DISCRETE_FRAMER_HH_
#define _H265_VIDEO_LIVE_DISCRETE_FRAMER_HH_

#include "H265VideoStreamFramer.hh"

class H265VideoLiveDiscreteFramer :public H265VideoStreamFramer//H265VideoStreamDiscreteFramer
{

public:
	static H265VideoLiveDiscreteFramer*
		createNew(UsageEnvironment& env, FramedSource* inputSource);

protected:
	H265VideoLiveDiscreteFramer(UsageEnvironment& env, FramedSource* inputSource);
	// called only by createNew()
	virtual ~H265VideoLiveDiscreteFramer();


protected:
	// redefined virtual functions:
	virtual void doGetNextFrame();

protected:

	static void afterGettingFrame(void* clientData, unsigned frameSize,
		unsigned numTruncatedBytes,
	struct timeval presentationTime,
		unsigned durationInMicroseconds);
	void afterGettingFrame1(unsigned frameSize,
		unsigned numTruncatedBytes,
	struct timeval presentationTime,
		unsigned durationInMicroseconds);

protected:
	struct timeval fNewPresentationTime;
};

#endif