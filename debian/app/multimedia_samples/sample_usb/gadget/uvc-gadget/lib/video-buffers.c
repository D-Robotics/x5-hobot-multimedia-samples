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
 * Video buffers
 *
 * Copyright (C) 2018 Laurent Pinchart
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */

#include "video-buffers.h"

#include <stdlib.h>
#include <string.h>

struct video_buffer_set *video_buffer_set_new(unsigned int nbufs)
{
	struct video_buffer_set *buffers;

	buffers = malloc(sizeof *buffers);
	if (!buffers)
		return NULL;

	buffers->nbufs = nbufs;
	buffers->buffers = calloc(nbufs, sizeof *buffers->buffers);
	if (!buffers->buffers) {
		free(buffers);
		return NULL;
	}

	return buffers;
}

void video_buffer_set_delete(struct video_buffer_set *buffers)
{
	if (!buffers)
		return;

	free(buffers->buffers);
	free(buffers);
}
