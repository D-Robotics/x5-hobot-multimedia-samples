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
// H.264 Video File sinks
// Implementation

#include "H264VideoFileSink.hh"
#include "OutputFile.hh"

////////// H264VideoFileSink //////////

H264VideoFileSink
::H264VideoFileSink(UsageEnvironment& env, FILE* fid,
		    char const* sPropParameterSetsStr,
		    unsigned bufferSize, char const* perFrameFileNamePrefix)
  : H264or5VideoFileSink(env, fid, bufferSize, perFrameFileNamePrefix,
			 sPropParameterSetsStr, NULL, NULL) {
}

H264VideoFileSink::~H264VideoFileSink() {
}

H264VideoFileSink*
H264VideoFileSink::createNew(UsageEnvironment& env, char const* fileName,
			     char const* sPropParameterSetsStr,
			     unsigned bufferSize, Boolean oneFilePerFrame) {
  do {
    FILE* fid;
    char const* perFrameFileNamePrefix;
    if (oneFilePerFrame) {
      // Create the fid for each frame
      fid = NULL;
      perFrameFileNamePrefix = fileName;
    } else {
      // Normal case: create the fid once
      fid = OpenOutputFile(env, fileName);
      if (fid == NULL) break;
      perFrameFileNamePrefix = NULL;
    }

    return new H264VideoFileSink(env, fid, sPropParameterSetsStr, bufferSize, perFrameFileNamePrefix);
  } while (0);

  return NULL;
}
