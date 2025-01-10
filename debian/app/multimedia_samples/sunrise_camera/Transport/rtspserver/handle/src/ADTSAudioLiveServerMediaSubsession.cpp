// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
// on demand, from a H264 video file.
// Implementation


#include "MPEG4GenericRTPSink.hh"
#include "ADTSAudioLiveServerMediaSubsession.hh"
#include "ADTSAudioSource.hh"
#include "utils/utils_log.h"

ADTSAudioLiveServerMediaSubsession*
ADTSAudioLiveServerMediaSubsession::createNew(UsageEnvironment& env, Boolean reuseFirstSource) {
	return new ADTSAudioLiveServerMediaSubsession(env, reuseFirstSource);
}

ADTSAudioLiveServerMediaSubsession::ADTSAudioLiveServerMediaSubsession(UsageEnvironment& env, Boolean reuseFirstSource)
	: OnDemandServerMediaSubsession(env, True /*reuse the first source*/){
}

ADTSAudioLiveServerMediaSubsession::~ADTSAudioLiveServerMediaSubsession() {
}

void ADTSAudioLiveServerMediaSubsession::startStream(unsigned clientSessionId,
						void* streamToken,
						TaskFunc* rtcpRRHandler,
						void* rtcpRRHandlerClientData,
						unsigned short& rtpSeqNum,
						unsigned& rtpTimestamp,
						ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
						void* serverRequestAlternativeByteHandlerClientData) {
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

FramedSource* ADTSAudioLiveServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
	estBitrate = 96; // kbps, estimate

	ADTSAudioSource *buffsource = ADTSAudioSource::createNew(envir(), 1, 4, 2);
	if (buffsource == NULL) return NULL;

	return buffsource;
}

RTPSink* ADTSAudioLiveServerMediaSubsession::createNewRTPSink(
	Groupsock* rtpGroupsock,
	unsigned char rtpPayloadTypeIfDynamic,
	FramedSource* inputSource) 
{
	ADTSAudioSource* adtsSource = (ADTSAudioSource*)inputSource;
	return MPEG4GenericRTPSink::createNew(envir(), rtpGroupsock,
		rtpPayloadTypeIfDynamic,
		adtsSource->samplingFrequency(),
		"audio", "AAC-hbr", adtsSource->configStr(),
		adtsSource->numChannels());
}
