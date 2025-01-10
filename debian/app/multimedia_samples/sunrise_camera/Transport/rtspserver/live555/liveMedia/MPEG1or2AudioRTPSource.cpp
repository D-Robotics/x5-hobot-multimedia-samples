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
Free Software Foundation; either version 3 of the License, or (at your
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
// Copyright (c) 1996-2019 Live Networks, Inc.  All rights reserved.
// MPEG-1 or MPEG-2 Audio RTP Sources
// Implementation

#include "MPEG1or2AudioRTPSource.hh"

MPEG1or2AudioRTPSource*
MPEG1or2AudioRTPSource::createNew(UsageEnvironment& env,
			      Groupsock* RTPgs,
			      unsigned char rtpPayloadFormat,
			      unsigned rtpTimestampFrequency) {
  return new MPEG1or2AudioRTPSource(env, RTPgs, rtpPayloadFormat,
				rtpTimestampFrequency);
}

MPEG1or2AudioRTPSource::MPEG1or2AudioRTPSource(UsageEnvironment& env,
				       Groupsock* rtpGS,
				       unsigned char rtpPayloadFormat,
				       unsigned rtpTimestampFrequency)
  : MultiFramedRTPSource(env, rtpGS,
			 rtpPayloadFormat, rtpTimestampFrequency) {
}

MPEG1or2AudioRTPSource::~MPEG1or2AudioRTPSource() {
}

Boolean MPEG1or2AudioRTPSource
::processSpecialHeader(BufferedPacket* packet,
                       unsigned& resultSpecialHeaderSize) {
  // There's a 4-byte header indicating fragmentation.
  if (packet->dataSize() < 4) return False;

  // Note: This fragmentation header is actually useless to us, because
  // it doesn't tell us whether or not this RTP packet *ends* a
  // fragmented frame.  Thus, we can't use it to properly set
  // "fCurrentPacketCompletesFrame".  Instead, we assume that even
  // a partial audio frame will be usable to clients.

  resultSpecialHeaderSize = 4;
  return True;
}

char const* MPEG1or2AudioRTPSource::MIMEtype() const {
  return "audio/MPEG";
}

