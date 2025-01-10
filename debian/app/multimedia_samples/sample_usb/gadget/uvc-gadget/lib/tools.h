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
 * Miscellaneous Tools
 *
 * Copyright (C) 2018 Laurent Pinchart
 *
 * This file comes from the omap3-isp-live project
 * (git://git.ideasonboard.org/omap3-isp-live.git)
 *
 * Copyright (C) 2010-2011 Ideas on board SPRL
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */

#ifndef __TOOLS_H__
#define __TOOLS_H__

#define ARRAY_SIZE(array)	(sizeof(array) / sizeof((array)[0]))

#define min(a, b) ({				\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	__a < __b ? __a : __b;			\
})

#define min_t(type, a, b) ({			\
	type __a = (a);				\
	type __b = (b);				\
	__a < __b ? __a : __b;			\
})

#define max(a, b) ({				\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	__a > __b ? __a : __b;			\
})

#define max_t(type, a, b) ({			\
	type __a = (a);				\
	type __b = (b);				\
	__a > __b ? __a : __b;			\
})

#define clamp(val, min, max) ({			\
	typeof(val) __val = (val);		\
	typeof(min) __min = (min);		\
	typeof(max) __max = (max);		\
	__val = __val < __min ? __min : __val;	\
	__val > __max ? __max : __val;		\
})

#define clamp_t(type, val, min, max) ({		\
	type __val = (val);			\
	type __min = (min);			\
	type __max = (max);			\
	__val = __val < __min ? __min : __val;	\
	__val > __max ? __max : __val;		\
})

#define div_round_up(num, denom)	(((num) + (denom) - 1) / (denom))

#define container_of(ptr, type, member) \
	(type *)((char *)(ptr) - offsetof(type, member))

#endif /* __TOOLS_H__ */
