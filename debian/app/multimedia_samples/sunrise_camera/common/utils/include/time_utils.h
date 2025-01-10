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

#ifndef __TIME_UTILS__
#define __TIME_UTILS__

#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#ifdef __cplusplus
extern "C"{
#endif

struct TimeStatistics
{
    uint64_t last_ms;
    uint64_t start_ms;
    uint64_t end_ms;
    uint64_t loop_period_ms;
};
void time_statistics_at_beginning_of_loop(struct TimeStatistics *statistics);
void time_statistics_at_ending_of_loop(struct TimeStatistics *statistics);
void time_statistics_info_show(struct TimeStatistics *statistics, const char *tag, bool is_open);

uint64_t get_timestamp_ms();

void get_world_time_string(char *time_buffer, int time_buffer_size);
#ifdef __cplusplus
}
#endif

#endif
