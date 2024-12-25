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

#ifndef SHA256_H
#define SHA256_H

#ifndef uint8
#define uint8  unsigned char
#endif

#ifndef uint32
#define uint32 unsigned long int
#endif

typedef struct
{
    uint32 total[2];
    uint32 state[8];
    uint8 buffer[64];
}sha256_context;

void sha256_starts(sha256_context *ctx );
void sha256_process(sha256_context *ctx, uint8 data[64]);
void sha256_update(sha256_context *ctx, uint8 *input, uint32 length );
void sha256_finish(sha256_context *ctx, uint8 digest[32]);
void sha256_mac(unsigned char *data, unsigned int dataLen, unsigned char mac[32]);

#endif