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
// on demand, from a H265 Elementary Stream video file.
// C++ header

#ifndef _H265_VIDEO_LIVE_SERVER_MEDIA_SUBSESSION_HH
#define _H265_VIDEO_LIVE_SERVER_MEDIA_SUBSESSION_HH

#ifndef _ON_DEMAND_SERVER_MEDIA_SUBSESSION_HH
#include "OnDemandServerMediaSubsession.hh"
#endif
#include "H265MainVideoSource.hh"

class H265VideoLiveServerMediaSubsession : public OnDemandServerMediaSubsession {
public:
	static H265VideoLiveServerMediaSubsession*
		createNew(UsageEnvironment& env, Boolean reuseFirstSource,
			char *shmId, char *shmName, int streamBufSize, int frameRate,
			int buffer_region_size, int buffer_item_count);

	// Used to implement "getAuxSDPLine()":
	void checkForAuxSDPLine1();
	void afterPlayingDummy1();

protected:
	H265VideoLiveServerMediaSubsession(UsageEnvironment& env, Boolean reuseFirstSource,
		char *shmId, char *shmName, int streamBufSize, int frameRate,
		int buffer_region_size, int buffer_item_count);
	// called only by createNew();
	virtual ~H265VideoLiveServerMediaSubsession();

	void setDoneFlag() { fDoneFlag = ~0; }

	virtual void startStream(unsigned clientSessionId, void* streamToken,
			TaskFunc* rtcpRRHandler,
			void* rtcpRRHandlerClientData,
			unsigned short& rtpSeqNum,
					unsigned& rtpTimestamp,
		   ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
					void* serverRequestAlternativeByteHandlerClientData);
	virtual void pauseStream(unsigned /*clientSessionId*/, void* streamToken);
	virtual void seekStream(unsigned clientSessionId, void* streamToken, double& seekNPT, double streamDuration, u_int64_t& numBytes);
	virtual void seekStream(unsigned clientSessionId, void* streamToken, char*& absStart, char*& absEnd);


protected: // redefined virtual functions
	virtual char const* getAuxSDPLine(RTPSink* rtpSink,
		FramedSource* inputSource);
	virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
		unsigned& estBitrate);
	virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
		unsigned char rtpPayloadTypeIfDynamic,
		FramedSource* inputSource);

private:
	Boolean fReuseFirstSource;
	char* fAuxSDPLine;
	char fDoneFlag; // used when setting up "fAuxSDPLine"
	RTPSink* fDummyRTPSink; // ditto
	H265MainVideoSource* fVideoSource;
	char fShmId[32];
	char fShmName[32];
	int fStreamBufSize;
	int fFrameRate;
	int fBufferRegionSize;
	int fBufferItemCount;
	int fDummyVideoSourceCount;
};

#endif
