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

#ifndef  __BPU_COMMON_H__
#define __BPU_COMMON_H__

typedef struct{
	int x;
	int y;
	int width;
	int height;
	const char *label;
} detect_object_t;


#define DETECT_OBJECT_COUNT 100
typedef struct{
	int valid_count;
	detect_object_t detect_objects[DETECT_OBJECT_COUNT];
} detect_object_array_t;

#endif // ! __BPU_COMMON_H__