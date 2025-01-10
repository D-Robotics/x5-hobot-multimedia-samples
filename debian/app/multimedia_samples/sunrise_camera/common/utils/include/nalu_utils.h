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

#ifndef NALU_UTILS_HH
#define NALU_UTILS_HH
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
typedef struct
{
	int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
	unsigned len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
	unsigned max_size;            //! NAL Unit Buffer size
	int forbidden_bit;            //! Should always be FALSE
	int nal_reference_idc;        //! NALU_PRIORITY_xxxx
	int nal_unit_type;            //! NALU_TYPE_xxxx
	unsigned char *buf;                    //! contains the first byte followed by the EBSP
	unsigned short lost_packets;  //! true, if packet loss is detected
} NALU_t;

int find_start_code2(unsigned char *data);
int find_start_code3(unsigned char *data);
int get_annexb_nalu(unsigned char *frame, int length, NALU_t *nalu, int is_h265);
int nalu_is_beyond_source_data_range(unsigned char *nalu_start, int32_t nalu_length,
	unsigned char *src_start, int32_t src_length, const char* tag_for_debug);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif
