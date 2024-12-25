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
 * UVC stream handling
 *
 * Copyright (C) 2010-2018 Laurent Pinchart
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */

#ifndef __STREAM_H__
#define __STREAM_H__

struct events;
struct uvc_function_config;
struct uvc_stream;
struct v4l2_pix_format;
struct video_source;

/*
 * uvc_stream_new - Create a new UVC stream
 * @uvc_device: Filename of UVC device node
 *
 * Create a new UVC stream to handle the UVC function corresponding to the video
 * device node @uvc_device.
 *
 * Streams allocated with this function can be deleted with uvc_stream_delete().
 *
 * On success, returns a pointer to newly allocated and populated struct uvc_stream.
 * On failure, returns NULL.
 */
struct uvc_stream *uvc_stream_new(const char *uvc_device);

/*
 * uvc_stream_init_uvc - Initialize a UVC stream
 * @stream: the UVC stream
 * @fc: UVC function configuration
 *
 * Before it can be used, a UVC stream has to be initialized by calling this
 * function with a UVC function configuration @fc. The function configuration
 * contains all the parameters of the UVC function that will be handled by the
 * UVC stream. It can be parsed from the UVC function ConfigFS directory using
 * configfs_parse_uvc_function().
 *
 * uvc_stream_init_uvc() also registers UVC event notifiers for the stream. The
 * caller must have called the uvc_stream_set_event_handler() function first,
 * and ensure that the event handler is immediately usable. If the event loop is
 * already running, all initialization steps required to handle events must be
 * fully performed before calling this function.
 */
void uvc_stream_init_uvc(struct uvc_stream *stream,
			 struct uvc_function_config *fc);

/*
 * uvc_stream_set_event_handler - Set an event handler for a stream
 * @stream: the UVC stream
 * @events: the event handler
 *
 * This function sets the event handler that the stream can use to be notified
 * of file descriptor events.
 */
void uvc_stream_set_event_handler(struct uvc_stream *stream,
				  struct events *events);

void uvc_stream_set_video_source(struct uvc_stream *stream,
				 struct video_source *src);

/*
 * uvc_stream_delete - Delete a UVC stream
 * @stream: the UVC stream
 *
 * This functions deletes the @stream created with uvc_stream_new(). Upon return
 * the stream object may be freed, the @stream pointer thus becomes invalid and
 * the stream must not be touched anymore.
 *
 * Every stream allocated with uvc_stream_new() must be deleted when not needed
 * anymore.
 */
void uvc_stream_delete(struct uvc_stream *stream);

/*
 * uvc_stream_set_format - Set the active video format for the stream
 * @stream: the UVC stream
 * @format: the video stream format
 *
 * This function is called from the UVC protocol handler to configure the video
 * format for the @stream. It must not be called directly by applications.
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int uvc_stream_set_format(struct uvc_stream *stream,
			  const struct v4l2_pix_format *format);

/*
 * uvc_stream_set_frame_rate - Set the frame rate for the stream
 * @stream: the UVC stream
 * @fps:    the frame rate in frames per second
 *
 * This function is called from the UVC protocol handler to configure the frame
 * rate for the video source of the @stream. It must not be called directly by
 * applications.
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int uvc_stream_set_frame_rate(struct uvc_stream *stream, unsigned int fps);

/*
 * uvc_stream_enable - Turn on/off video streaming for the UVC stream
 * @stream: the UVC stream
 * @enable: 0 to stop the stream, 1 to start it
 *
 * This function is called from the UVC protocol handler to start video transfer
 * for the @stream. It must not be called directly by applications.
 */
void uvc_stream_enable(struct uvc_stream *stream, int enable);

#endif /* __STREAM_H__ */
