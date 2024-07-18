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

#include "SimpleRTPSink.hh"
#include "PCMAAudioLiveServerMediaSubsession.hh"
#include "uLawAudioFilter.hh"

#include "utils/utils_log.h"

static unsigned const samplingFrequencyTable[16] =
{
	96000, 88200, 64000, 48000,
	44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000,
	7350, 0, 0, 0
};

PCMAAudioLiveServerMediaSubsession*
PCMAAudioLiveServerMediaSubsession::createNew(UsageEnvironment& env, Boolean reuseFirstSource,
	u_int8_t bitsPerSample, 
	u_int8_t samplingFrequencyIndex, 
	u_int8_t channelConfiguration) {
	return new PCMAAudioLiveServerMediaSubsession(env, reuseFirstSource, bitsPerSample, samplingFrequencyIndex, channelConfiguration);
}

PCMAAudioLiveServerMediaSubsession::PCMAAudioLiveServerMediaSubsession(UsageEnvironment& env, Boolean reuseFirstSource,
		u_int8_t bitsPerSample, 
		u_int8_t samplingFrequencyIndex, 
		u_int8_t channelConfiguration)
	: OnDemandServerMediaSubsession(env, True/*reuse the first source*/, 6972, True){
	
	fBitsPerSample = bitsPerSample;
	fSamplingFrequencyIndex = samplingFrequencyIndex;
	fNumChannels = channelConfiguration;
}

PCMAAudioLiveServerMediaSubsession::~PCMAAudioLiveServerMediaSubsession() {
}

void PCMAAudioLiveServerMediaSubsession::startStream(unsigned clientSessionId,
						void* streamToken,
						TaskFunc* rtcpRRHandler,
						void* rtcpRRHandlerClientData,
						unsigned short& rtpSeqNum,
						unsigned& rtpTimestamp,
						ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
						void* serverRequestAlternativeByteHandlerClientData) {

	//sync
	fAudioSource->sync();
	
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


FramedSource* PCMAAudioLiveServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {

	fAudioSource = PCMAAudioSource::createNew(envir(), fBitsPerSample, fSamplingFrequencyIndex, fNumChannels);
	if (fAudioSource == NULL) return NULL;

	unsigned bitsPerSecond = fAudioSource->samplingFrequency()*fAudioSource->bitsPerSample()*fAudioSource->numChannels();
	estBitrate = (bitsPerSecond+500)/1000;; // kbps, estimate
	
	return fAudioSource;
}

RTPSink* PCMAAudioLiveServerMediaSubsession::createNewRTPSink(
	Groupsock* rtpGroupsock,
	unsigned char rtpPayloadTypeIfDynamic,
	FramedSource* inputSource) 
{
	SC_LOGI("fSamplingFrequency:%d fNumChannels:%d", samplingFrequencyTable[fSamplingFrequencyIndex], fNumChannels);
	return SimpleRTPSink::createNew(envir(), rtpGroupsock, 
		8, 
		samplingFrequencyTable[fSamplingFrequencyIndex], 
		"audio", "PCMA", 
		fNumChannels, False);//for test
}
