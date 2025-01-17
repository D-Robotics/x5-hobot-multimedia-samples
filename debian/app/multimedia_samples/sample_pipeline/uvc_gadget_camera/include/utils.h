/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * utils.h
 *	utils for debug & trace
 *
 * Copyright (C) 2024 D-Robotics Inc.
 *
 * Contact: jianghe xu<jianghe.xu@d-robotics.cc>
 */

#ifndef __USB_CAMERA_UTILS_H__
#define __USB_CAMERA_UTILS_H__

#include <time.h>

/*##################### TRACE & DEBUG #######################*/
#define TRACE_ON 0
// #undef TRCACE_ON	// uncomment it to enable function trace
#define trace_in() \
	if (TRACE_ON) \
		printf("##function %s in\n", __func__); \

#define trace_out() \
	if (TRACE_ON) \
		printf("##function %s succeed\n", __func__); \

#endif	/* __USB_CAMERA_UTILS_H__ */
