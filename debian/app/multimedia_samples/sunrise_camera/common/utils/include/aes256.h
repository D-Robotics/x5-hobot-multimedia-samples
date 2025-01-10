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

#ifndef __ENCRYPT_DEF__
#define __ENCRYPT_DEF__

#ifdef __cplusplus
extern "C" {
#endif

long aes256_cbc_enc_by_head(unsigned char *head, unsigned int headLen, unsigned char *data, unsigned int dataLen,
		unsigned char **encData, unsigned int *encDataLen, unsigned char key[32]);
long aes256_cbc_enc(unsigned char *data, unsigned int dataLen, unsigned char **encData, unsigned int *encDataLen, unsigned char key[32]);
long aes256_cbc_dec(unsigned char *data, unsigned int dataLen, unsigned char **decData, unsigned int *decDataLen, unsigned char key[32]);


#ifdef __cplusplus
}
#endif

#endif
