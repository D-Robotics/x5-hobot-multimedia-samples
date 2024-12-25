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
 * glob() compatibility
 *
 * Copyright (C) 2018 Laurent Pinchart
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */
#ifndef __UVC_GADGET_GLOB_H__
#define __UVC_GADGET_GLOB_H__

#include "config.h"

#ifdef HAVE_GLOB
#include_next <glob.h>
#else
#include "compat/glob.h"
#endif

#endif /* __UVC_GADGET_GLOB_H__ */
