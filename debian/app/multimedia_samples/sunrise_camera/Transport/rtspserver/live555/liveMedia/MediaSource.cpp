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
// Media Sources
// Implementation

#include "MediaSource.hh"

////////// MediaSource //////////

MediaSource::MediaSource(UsageEnvironment& env)
	: Medium(env) {
}

MediaSource::~MediaSource() {
}

Boolean MediaSource::isSource() const {
  return True;
}

char const* MediaSource::MIMEtype() const {
  return "application/OCTET-STREAM"; // default type
}

Boolean MediaSource::isFramedSource() const {
  return False; // default implementation
}
Boolean MediaSource::isRTPSource() const {
  return False; // default implementation
}
Boolean MediaSource::isMPEG1or2VideoStreamFramer() const {
  return False; // default implementation
}
Boolean MediaSource::isMPEG4VideoStreamFramer() const {
  return False; // default implementation
}
Boolean MediaSource::isH264VideoStreamFramer() const {
  return False; // default implementation
}
Boolean MediaSource::isH265VideoStreamFramer() const {
  return False; // default implementation
}
Boolean MediaSource::isDVVideoStreamFramer() const {
  return False; // default implementation
}
Boolean MediaSource::isJPEGVideoSource() const {
  return False; // default implementation
}
Boolean MediaSource::isAMRAudioSource() const {
  return False; // default implementation
}

Boolean MediaSource::lookupByName(UsageEnvironment& env,
				  char const* sourceName,
				  MediaSource*& resultSource) {
  resultSource = NULL; // unless we succeed

  Medium* medium;
  if (!Medium::lookupByName(env, sourceName, medium)) return False;

  if (!medium->isSource()) {
    env.setResultMsg(sourceName, " is not a media source");
    return False;
  }

  resultSource = (MediaSource*)medium;
  return True;
}

void MediaSource::getAttributes() const {
  // Default implementation
  envir().setResultMsg("");
}
