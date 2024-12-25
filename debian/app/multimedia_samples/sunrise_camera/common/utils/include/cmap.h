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

#ifndef C_MAP_H
#define C_MAP_H
#include "lock_utils.h"

typedef union
{
	int i_key;
	char p_key[64];
}cmap_key;

typedef struct cmapnode_s{
	cmap_key key;
	void* data;
	struct cmapnode_s *next;
}cmapnode;
typedef struct
{
	cmapnode *front;
	cmapnode *rear;
	int size;
	CMtx lock;
}cmap;

#ifdef __cplusplus
extern "C"{
#endif
void cmap_init(cmap *p);
//将释放节点数据内存
void cmap_clear(cmap *q);
//将释放节点数据内存
int cmap_destory(cmap *q);
int cmap_is_empty(cmap *q);
int cmap_ikey_insert(cmap *q, int key, void* e);
//将释放节点数据内存
int cmap_ikey_erase(cmap *q, int key);
void* cmap_ikey_find(cmap *q, int key);
int cmap_pkey_insert(cmap *q, const char* key, void* e);
//将释放节点数据内存
int cmap_pkey_erase(cmap *q, const char* key);
void* cmap_pkey_find(cmap *q, const char* key);
cmapnode* cmap_index_get(cmap *q, int index);
int cmap_size(cmap *q);
#ifdef __cplusplus
}
#endif

#endif