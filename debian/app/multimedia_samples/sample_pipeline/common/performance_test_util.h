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