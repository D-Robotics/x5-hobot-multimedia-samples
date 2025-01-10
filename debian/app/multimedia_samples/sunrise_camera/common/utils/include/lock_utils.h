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

#ifndef LOCK_UTILS_H
#define LOCK_UTILS_H

#include <pthread.h> 
#include <semaphore.h>

#ifdef __cplusplus
extern "C"{
#endif

///////////////////////////////////////////////////////////////////
//信号量
typedef void* CSem;
CSem  csem_create(int initial_count, int maxcount);
CSem  csem_open(char* name, int initial_count);
int   csem_delete(CSem handle);
int   csem_close(CSem handle);
int   csem_wait(CSem handle);
int   csem_wait_timeout(void* handle, unsigned int timeout/*毫秒*/);
int   csem_post(CSem handle);
int   csem_getcount(CSem handle, int *count);


///////////////////////////////////////////////////////////////////
//互斥锁
typedef void* CMtx;
CMtx cmtx_create();
void cmtx_delete(CMtx handle);
void cmtx_enter(CMtx handle);
void cmtx_leave(CMtx handle);

#ifdef __cplusplus
}
#endif

#endif
