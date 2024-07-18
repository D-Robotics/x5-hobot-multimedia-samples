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
