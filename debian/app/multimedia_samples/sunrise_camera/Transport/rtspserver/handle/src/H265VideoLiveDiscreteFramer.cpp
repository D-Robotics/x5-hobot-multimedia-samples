#include "H265VideoLiveDiscreteFramer.hh"
#include "MPEGVideoStreamFramer.hh"
#include "utils/utils_log.h"


H265VideoLiveDiscreteFramer* H265VideoLiveDiscreteFramer::createNew(UsageEnvironment& env, FramedSource* inputSource)
{
	return new H265VideoLiveDiscreteFramer(env, inputSource);
}

H265VideoLiveDiscreteFramer::H265VideoLiveDiscreteFramer(UsageEnvironment& env, FramedSource* inputSource)
	: H265VideoStreamFramer(env, inputSource, False/*don't create a parser*/, False) {

	fNewPresentationTime.tv_sec = 0;
	fNewPresentationTime.tv_usec = 0;
}

H265VideoLiveDiscreteFramer::~H265VideoLiveDiscreteFramer()
{
	fFrameRate = 30.0;
}

void H265VideoLiveDiscreteFramer::doGetNextFrame() {
	// Arrange to read data (which should be a complete H.264 or H.265 NAL unit)
	// from our data source, directly into the client's input buffer.
	// After reading this, we'll do some parsing on the frame.
	fInputSource->getNextFrame(fTo, fMaxSize,
		afterGettingFrame, this,
		FramedSource::handleClosure, this);
}

void H265VideoLiveDiscreteFramer::afterGettingFrame(void* clientData,
		unsigned frameSize,
		unsigned numTruncatedBytes,
		struct timeval presentationTime,
		unsigned durationInMicroseconds) {
	H265VideoLiveDiscreteFramer* source = (H265VideoLiveDiscreteFramer*)clientData;
	source->afterGettingFrame1(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void H265VideoLiveDiscreteFramer
::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
struct timeval presentationTime,
	unsigned durationInMicroseconds) {

		
#if 1  // debug for vlc player, delete by liyaqiang
	// Get the "nal_unit_type", to see if this NAL unit is one that we want to save a copy of:
	u_int8_t nal_unit_type;
	if (fHNumber == 264 && frameSize >= 1) {
		nal_unit_type = fTo[0] & 0x1F;
	}
	else if (fHNumber == 265 && frameSize >= 2) {
		nal_unit_type = (fTo[0] & 0x7E) >> 1;
	}
	else {
		// This is too short to be a valid NAL unit, so just assume a bogus nal_unit_type
		nal_unit_type = 0xFF;
	}

	// Begin by checking for a (likely) common error: NAL units that (erroneously) begin with a
	// 0x00000001 or 0x000001 'start code'.  (Those start codes should only be in byte-stream data;
	// *not* data that consists of discrete NAL units.)
	// Once again, to be clear: The NAL units that you feed to a "H265or5VideoStreamDiscreteFramer"
	// MUST NOT include start codes.
	if (frameSize >= 4 && fTo[0] == 0 && fTo[1] == 0 && ((fTo[2] == 0 && fTo[3] == 1) || fTo[2] == 1)) {
		envir() << "H265or5VideoStreamDiscreteFramer error: MPEG 'start code' seen in the input\n";
	}
	else if (isVPS(nal_unit_type)) { // Video parameter set (VPS)
		saveCopyOfVPS(fTo, frameSize);
	}
	else if (isSPS(nal_unit_type)) { // Sequence parameter set (SPS)
		saveCopyOfSPS(fTo, frameSize);
	}
	else if (isPPS(nal_unit_type)) { // Picture parameter set (PPS)
		saveCopyOfPPS(fTo, frameSize);
	}

	fPictureEndMarker = fHNumber == 264
		? (nal_unit_type <= 5 && nal_unit_type > 0)
		: (nal_unit_type <= 31);
#endif

	// Finally, complete delivery to the client:
	fFrameSize = frameSize;
	fNumTruncatedBytes = numTruncatedBytes;
	fPresentationTime = presentationTime;

	if (isSPS(nal_unit_type) || isPPS(nal_unit_type))
	{
		fDurationInMicroseconds = 0;
	}
	else
	{
		fDurationInMicroseconds = durationInMicroseconds;
	}

	afterGetting(this);
}

