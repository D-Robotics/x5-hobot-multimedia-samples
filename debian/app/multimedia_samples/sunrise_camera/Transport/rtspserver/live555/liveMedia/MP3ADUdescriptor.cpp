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
// Descriptor preceding frames of 'ADU' MP3 streams (for improved loss-tolerance)
// Implementation

#include "MP3ADUdescriptor.hh"

////////// ADUdescriptor //////////

//##### NOTE: For now, ignore fragmentation.  Fix this later! #####

#define TWO_BYTE_DESCR_FLAG 0x40

unsigned ADUdescriptor::generateDescriptor(unsigned char*& toPtr,
					   unsigned remainingFrameSize) {
  unsigned descriptorSize = ADUdescriptor::computeSize(remainingFrameSize);
  switch (descriptorSize) {
  case 1: {
    *toPtr++ = (unsigned char)remainingFrameSize;
    break;
  }
  case 2: {
    generateTwoByteDescriptor(toPtr, remainingFrameSize);
    break;
  }
  }

  return descriptorSize;
}

void ADUdescriptor::generateTwoByteDescriptor(unsigned char*& toPtr,
					      unsigned remainingFrameSize) {
  *toPtr++ = (TWO_BYTE_DESCR_FLAG|(unsigned char)(remainingFrameSize>>8));
  *toPtr++ = (unsigned char)(remainingFrameSize&0xFF);
}

unsigned ADUdescriptor::getRemainingFrameSize(unsigned char*& fromPtr) {
  unsigned char firstByte = *fromPtr++;

  if (firstByte&TWO_BYTE_DESCR_FLAG) {
    // This is a 2-byte descriptor
    unsigned char secondByte = *fromPtr++;

    return ((firstByte&0x3F)<<8) | secondByte;
  } else {
    // This is a 1-byte descriptor
    return (firstByte&0x3F);
  }
}

