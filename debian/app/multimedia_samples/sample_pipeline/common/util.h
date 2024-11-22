#ifndef __UTIL_H
#define __UTIL_H
#include <stdint.h>
#include <sys/time.h>

inline static uint64_t get_timestamp_ms()
{
	uint64_t timestamp;
	struct timeval ts;

	gettimeofday(&ts, NULL);
	timestamp = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_usec / 1000;
	return timestamp;
}
#endif // !__UTIL_H