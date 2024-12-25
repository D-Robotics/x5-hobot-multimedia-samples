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

#include <sys/time.h>
#include "performance_test_util.h"

static uint64_t get_timestamp_us()
{
	uint64_t timestamp;
	struct timeval ts;

	gettimeofday(&ts, NULL);
	timestamp = (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_usec;
	return timestamp;
}

void performance_test_start_simple(struct PerformanceTestParamSimple *param)
{
	if(param->run_count == 0){
		param->test_start_time_us = get_timestamp_us();
	}
}

void performance_test_stop_simple(struct PerformanceTestParamSimple *param)
{
	param->run_count++;
	if(param->run_count >= param->iteration_number){
		param->test_end_time_us = get_timestamp_us();
		uint64_t diff_time_sum_us = param->test_end_time_us - param->test_start_time_us;
		float fps = 1000000.0 * param->iteration_number / diff_time_sum_us;
		param->test_count++;
		printf("\n[%d] [Case %-30s] [TestTimes %5d] [TotalConsume %-10ldus] [AverageConsume %-7ldus] [FPS %7.1f]\n",
			param->test_count,
			param->test_case, param->iteration_number,
			diff_time_sum_us, diff_time_sum_us / param->iteration_number,
			fps);

		param->run_count = 0;
	}
}


void performance_test_start(struct PerformanceTestParam *param)
{
	param->test_start_time_us = get_timestamp_us();
}

void performance_test_stop(struct PerformanceTestParam *param)
{
	uint64_t current_us = get_timestamp_us();
	uint64_t diff = current_us - param->test_start_time_us;

	param->consumu_time_sum_us += diff;
	param->run_count++;

	if(param->run_count >= param->iteration_number){

		uint64_t diff_time_sum_us = param->consumu_time_sum_us;
		float fps = 1000000.0 * param->iteration_number / diff_time_sum_us;
		param->test_count++;

		printf("\n[%d] [Case %-30s] [TestTimes %5d] [TotalConsume %-10ldus] [AverageConsume %-7ldus] [FPS %7.1f]\n",
			param->test_count,
			param->test_case, param->iteration_number,
			diff_time_sum_us, diff_time_sum_us / param->iteration_number,
			fps);

		param->run_count = 0;
		param->consumu_time_sum_us = 0;
	}
}