/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2014 Live Networks, Inc.  All rights reserved.
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from a H265 video file.
// Implementation


#include "H265VideoRTPSink.hh"
#include "H265VideoStreamFramer.hh"

#include "H265VideoLiveServerMediaSubsession.hh"
#include "H265VideoLiveDiscreteFramer.hh"
#include "utils/utils_log.h"

H265VideoLiveServerMediaSubsession*
H265VideoLiveServerMediaSubsession::createNew(UsageEnvironment& env, Boolean reuseFirstSource,
	char *shmId, char *shmName, int streamBufSize, int frameRate,
	int buffer_region_size, int buffer_item_count) {
	return new H265VideoLiveServerMediaSubsession(env, reuseFirstSource, shmId, shmName, streamBufSize, frameRate,
		buffer_region_size, buffer_item_count);
}

H265VideoLiveServerMediaSubsession::H265VideoLiveServerMediaSubsession(UsageEnvironment& env, Boolean reuseFirstSource,
char *shmId, char *shmName, int streamBufSize, int frameRate,
	int buffer_region_size, int buffer_item_count)
	: OnDemandServerMediaSubsession(env, True/*reuse the first source*/, 6970, True),
	fAuxSDPLine(NULL), fDoneFlag(0), fDummyRTPSink(NULL) {
	// 外部的shm参数终于传进来了，后面有时间看看怎么传递会更合适吧
	// 使用 memcpy 函数复制字符串，并确保在目标字符串的末尾添加终止符
	memcpy(fShmId, shmId, strlen(shmId) + 1);
	memcpy(fShmName, shmName, strlen(shmName) + 1);

	fStreamBufSize = streamBufSize;
	fFrameRate = frameRate;
	fBufferRegionSize = buffer_region_size;
	fBufferItemCount = buffer_item_count;
	fDummyVideoSourceCount = 0;
	SC_LOGI("media subsession created for :%s [%d:%d]", shmName, fBufferRegionSize, fBufferItemCount);
}

H265VideoLiveServerMediaSubsession::~H265VideoLiveServerMediaSubsession() {
	SC_LOGI("media subsession destroyed for :%s", fShmName);
	delete[] fAuxSDPLine;
}

void H265VideoLiveServerMediaSubsession::startStream(unsigned clientSessionId,
						void* streamToken,
						TaskFunc* rtcpRRHandler,
						void* rtcpRRHandlerClientData,
						unsigned short& rtpSeqNum,
						unsigned& rtpTimestamp,
						ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
						void* serverRequestAlternativeByteHandlerClientData) {

	fVideoSource->idr();
	//sync
	fVideoSource->sync();

	StreamState* streamState = (StreamState*)streamToken;
	Destinations* destinations
		= (Destinations*)(fDestinationsHashTable->Lookup((char const*)clientSessionId));
	if (streamState != NULL)
	{
		streamState->startPlaying(destinations, clientSessionId,
				      rtcpRRHandler, rtcpRRHandlerClientData,
				      serverRequestAlternativeByteHandler, serverRequestAlternativeByteHandlerClientData);
		RTPSink* rtpSink = streamState->rtpSink(); // alias
		if (rtpSink != NULL)
		{
			rtpSeqNum = rtpSink->currentSeqNo();
			rtpTimestamp = rtpSink->presetNextTimestamp();
		}
	}

}


void H265VideoLiveServerMediaSubsession::pauseStream(unsigned /*clientSessionId*/,
						void* streamToken) {
	// Pausing isn't allowed if multiple clients are receiving data from
	// the same source:
	if (fReuseFirstSource) return;

	StreamState* streamState = (StreamState*)streamToken;
	if (streamState != NULL) streamState->pause();
}


void H265VideoLiveServerMediaSubsession::seekStream(unsigned /*clientSessionId*/,
					       void* streamToken, double& seekNPT, double streamDuration, u_int64_t& numBytes) {
	SC_LOGI("H265VideoLiveServerMediaSubsession::seekStream1");
	//call out size seek -- add by donyj

	numBytes = 0; // by default: unknown

	// Seeking isn't allowed if multiple clients are receiving data from the same source:
	if (fReuseFirstSource) return;

	StreamState* streamState = (StreamState*)streamToken;
	if (streamState != NULL && streamState->mediaSource() != NULL) {
		seekStreamSource(streamState->mediaSource(), seekNPT, streamDuration, numBytes);

		streamState->startNPT() = (float)seekNPT;
		RTPSink* rtpSink = streamState->rtpSink(); // alias
		if (rtpSink != NULL) rtpSink->resetPresentationTimes();
	}
}

void H265VideoLiveServerMediaSubsession::seekStream(unsigned /*clientSessionId*/,
					       void* streamToken, char*& absStart, char*& absEnd) {
	SC_LOGI("H265VideoLiveServerMediaSubsession::seekStream2");
	// Seeking isn't allowed if multiple clients are receiving data from the same source:
	if (fReuseFirstSource) return;

	StreamState* streamState = (StreamState*)streamToken;
	if (streamState != NULL && streamState->mediaSource() != NULL) {
		seekStreamSource(streamState->mediaSource(), absStart, absEnd);
	}
}


static void afterPlayingDummy(void* clientData) {
	H265VideoLiveServerMediaSubsession* subsess = (H265VideoLiveServerMediaSubsession*)clientData;
	subsess->afterPlayingDummy1();
}

void H265VideoLiveServerMediaSubsession::afterPlayingDummy1() {
	// Unschedule any pending 'checking' task:
	envir().taskScheduler().unscheduleDelayedTask(nextTask());
	// Signal the event loop that we're done:
	setDoneFlag();
}

static void checkForAuxSDPLine(void* clientData) {
	H265VideoLiveServerMediaSubsession* subsess = (H265VideoLiveServerMediaSubsession*)clientData;
	subsess->checkForAuxSDPLine1();
}

void H265VideoLiveServerMediaSubsession::checkForAuxSDPLine1() {
	char const* dasl;

	if (fAuxSDPLine != NULL) {
		// Signal the event loop that we're done:
		setDoneFlag();
	}
	else if (fDummyRTPSink != NULL && (dasl = fDummyRTPSink->auxSDPLine()) != NULL) {
		fAuxSDPLine = strDup(dasl);
		fDummyRTPSink = NULL;

		// Signal the event loop that we're done:
		setDoneFlag();
	}
	else if (!fDoneFlag) {
		// try again after a brief delay:
		int uSecsToDelay = 100000; // 100 ms
		nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
			(TaskFunc*)checkForAuxSDPLine, this);
	}
}

char const* H265VideoLiveServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
	if (fAuxSDPLine != NULL) return fAuxSDPLine; // it's already been set up (for a previous client)

	if (fDummyRTPSink == NULL) { // we're not already setting it up for another, concurrent stream
		// Note: For H265 video files, the 'config' information (used for several payload-format
		// specific parameters in the SDP description) isn't known until we start reading the file.
		// This means that "rtpSink"s "auxSDPLine()" will be NULL initially,
		// and we need to start reading data from our file until this changes.
		fDummyRTPSink = rtpSink;

		// Start reading the file:
		fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

		// Check whether the sink's 'auxSDPLine()' is ready:
		checkForAuxSDPLine(this);
	}

	envir().taskScheduler().doEventLoop(&fDoneFlag);

	return fAuxSDPLine;
}

FramedSource* H265VideoLiveServerMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) {
	bool is_dummy = false;
	char* fShmId_tmp = fShmId;
	if(clientSessionId == 0){
		fDummyVideoSourceCount++;
		is_dummy = true;
		int len = strlen(fShmId) + snprintf(NULL, 0, "-%d", fDummyVideoSourceCount) + 1;
		fShmId_tmp = (char*)malloc(len * sizeof(char));
		sprintf(fShmId_tmp, "%s-%d", fShmId, fDummyVideoSourceCount);
	}

	SC_LOGI("create stream source %s %s bitrate:%d clientSessionId:%d fDummyVideoSourceCount:%d",
		fShmId, fShmName, estBitrate, clientSessionId, fDummyVideoSourceCount);
	fVideoSource = H265MainVideoSource::createNew(envir(), fShmId, fShmName, fStreamBufSize, fFrameRate,
		fBufferRegionSize, fBufferItemCount, is_dummy);

	if(clientSessionId == 0){
		free(fShmId_tmp);
	}
	estBitrate = fBufferRegionSize; // kbps, estimate
	if (fVideoSource == NULL) {
		SC_LOGE("createNewStreamSource create video source failed.");
		return NULL;
	}

	H265VideoLiveDiscreteFramer* videoSource = H265VideoLiveDiscreteFramer::createNew(envir(), (FramedSource*)fVideoSource);
	if (videoSource == NULL) {
		SC_LOGE("createNewStreamSource create discrete framer failed.");
		return NULL;
	}
	return videoSource;
}

RTPSink* H265VideoLiveServerMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock,
		unsigned char rtpPayloadTypeIfDynamic,
		FramedSource* /*inputSource*/)
{
	return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
