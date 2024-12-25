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
 * V4L2 video source
 *
 * Copyright (C) 2018 Laurent Pinchart
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */
#ifndef __V4L2_VIDEO_SOURCE_H__
#define __V4L2_VIDEO_SOURCE_H__

#include "video-source.h"

struct events;
struct video_source;

struct video_source *v4l2_video_source_create(const char *devname);
void v4l2_video_source_init(struct video_source *src, struct events *events);

#endif /* __VIDEO_SOURCE_H__ */
