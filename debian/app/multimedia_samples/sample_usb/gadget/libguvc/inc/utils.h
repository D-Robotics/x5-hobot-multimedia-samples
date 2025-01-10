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

/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * utils.h
 *	utils for debug & trace
 *
 * Copyright (C) 2019 Horizon Robotics, Inc.
 *
 * Contact: jianghe xu<jianghe.xu@horizon.ai>
 */

#ifndef __USB_CAMERA_UTILS_H__
#define __USB_CAMERA_UTILS_H__

#include <time.h>

/*##################### TRACE & DEBUG #######################*/
#define TRACE_ON 1
// #undef TRCACE_ON	// uncomment it to enable function trace
#define trace_in() \
	if (TRACE_ON) \
		printf("##function %s in\n", __func__); \

#define trace_out() \
	if (TRACE_ON) \
		printf("##function %s succeed\n", __func__); \

#define interval_cost_trace_in() \
	struct timespec t1, t2; \
	static long t1_last = 0; \
				\
	clock_gettime(CLOCK_MONOTONIC, &t1); \
	long t1_ms = t1.tv_sec * 1000 + t1.tv_nsec / (1000 * 1000); \
	long intv = t1_ms - t1_last;

#define interval_cost_trace_out(buf_len) \
	clock_gettime(CLOCK_MONOTONIC, &t2); \
						\
	long t2_ms = t2.tv_sec * 1000 + t2.tv_nsec / (1000 * 1000); \
	long dur = t2_ms - t1_ms; \
	if (buf_len) { \
		printf("fill buffer(%d bytes) t1_ms(%ldms) t2_ms(%ldms) " \
			"interval (%ldms), and cost (%ldms)\n", \
			buf_len, t1_ms, t2_ms, intv, dur); \
	} else { \
		printf("fill buffer t1_ms(%ldms) t2_ms(%ldms) " \
			"interval (%ldms), and cost (%ldms)\n", \
			t1_ms, t2_ms, intv, dur); \
	} \
	t1_last = t1_ms;

/* log level */
#define UVC_LOG_ERR 5
#define UVC_LOG_WARRING 4
#define UVC_LOG_USER_INFO 3
#define UVC_LOG_INFO 2
#define UVC_LOG_DEBUG 1
#define UVC_LOG_TRACE 0

#define G_LOG_LEVEL UVC_LOG_TRACE

#define UVC_PRINTF(level, format, arg...)	\
	({ if (level >= G_LOG_LEVEL) 	\
		printf("" format, \
			 ## arg); 0; })

#endif	/* __USB_CAMERA_UTILS_H__ */
