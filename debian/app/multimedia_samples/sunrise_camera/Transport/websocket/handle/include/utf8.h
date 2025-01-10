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

/* 
 * File:   utf8.h
 * Author: Pontus Östlund <spam@poppa.se>
 *
 * UTF-8 encoding/decoding functions from/to ISO-8859-1 and US-ASCII.
 * The encoding/decoding functions here are taken from xml.c in PHP
 *
 * Copyright (c) 1999 - 2009 The PHP Group. All rights reserved.
 * Copyright (c) 2009        Pontus Östlund <spam@poppa.se>
 */

#ifndef _UTF8_H
#define	_UTF8_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef char XML_Char;
typedef struct {
  XML_Char        *name;
  char            (*decoding_function)(unsigned short);
  unsigned short  (*encoding_function)(unsigned char);
} xml_encoding;

char        *utf8_encode       (const char *in);
char        *utf8_decode       (const char *in);
void         utf8_clean        (void *str);
static char *xml_utf8_decode   (const XML_Char *, int, int *, const XML_Char *);
static char *xml_utf8_encode   (const char *s, int len, int *newlen,
                                const XML_Char *encoding);
static void *emalloc           (size_t size);
static void *erealloc          (void* ptr, size_t size);

#endif	/* _UTF8_H */
