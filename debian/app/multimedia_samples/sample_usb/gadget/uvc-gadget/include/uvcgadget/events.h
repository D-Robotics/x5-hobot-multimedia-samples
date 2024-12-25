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
 * Generic Event Handling
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

#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <stdbool.h>
#include <sys/select.h>

#include "list.h"

struct events {
	struct list_entry events;
	bool done;

	int maxfd;
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
};

enum event_type {
	EVENT_READ = 1,
	EVENT_WRITE = 2,
	EVENT_EXCEPTION = 4,
};

void events_watch_fd(struct events *events, int fd, enum event_type type,
		     void(*callback)(void *), void *priv);
void events_unwatch_fd(struct events *events, int fd, enum event_type type);

bool events_loop(struct events *events);
void events_stop(struct events *events);

void events_init(struct events *events);
void events_cleanup(struct events *events);

#endif
