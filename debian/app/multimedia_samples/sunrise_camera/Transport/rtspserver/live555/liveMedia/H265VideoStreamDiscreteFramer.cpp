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
// A simplified version of "H265VideoStreamFramer" that takes only complete,
// discrete frames (rather than an arbitrary byte stream) as input.
// This avoids the parsing and data copying overhead of the full
// "H265VideoStreamFramer".
// Implementation

#include "H265VideoStreamDiscreteFramer.hh"

H265VideoStreamDiscreteFramer*
H265VideoStreamDiscreteFramer::createNew(UsageEnvironment& env, FramedSource* inputSource,
					 Boolean includeStartCodeInOutput) {
  return new H265VideoStreamDiscreteFramer(env, inputSource, includeStartCodeInOutput);
}

H265VideoStreamDiscreteFramer
::H265VideoStreamDiscreteFramer(UsageEnvironment& env, FramedSource* inputSource,
				Boolean includeStartCodeInOutput)
  : H264or5VideoStreamDiscreteFramer(265, env, inputSource, includeStartCodeInOutput) {
}

H265VideoStreamDiscreteFramer::~H265VideoStreamDiscreteFramer() {
}

Boolean H265VideoStreamDiscreteFramer::isH265VideoStreamFramer() const {
  return True;
}
