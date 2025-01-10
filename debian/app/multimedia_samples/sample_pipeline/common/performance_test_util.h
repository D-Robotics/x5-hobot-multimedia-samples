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

#ifndef __PERFORMANCE_HH__
#define __PERFORMANCE_HH__
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
struct PerformanceTestParamSimple
{
	int test_count;
	int iteration_number;
	char *test_case; //test case's name

	uint64_t run_count;
	uint64_t test_start_time_us;
	uint64_t test_end_time_us;
};

void performance_test_start_simple(struct PerformanceTestParamSimple *param);
void performance_test_stop_simple(struct PerformanceTestParamSimple *param);


struct PerformanceTestParam
{
	int test_count;
	int iteration_number;
	char *test_case; //test case's name

	uint64_t run_count;
	uint64_t test_start_time_us;
	uint64_t consumu_time_sum_us;
};
void performance_test_start(struct PerformanceTestParam *param);
void performance_test_stop(struct PerformanceTestParam *param);
#endif