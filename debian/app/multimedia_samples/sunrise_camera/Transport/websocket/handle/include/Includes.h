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

/******************************************************************************
  Copyright (c) 2013 Morten Houmøller Nygaard - www.mortz.dk - admin@mortz.dk
 
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
******************************************************************************/

#ifndef _INCLUDES_H
#define _INCLUDES_H

#include <pthread.h> 			/* pthread_create, pthread_t, pthread_attr_t 
								   pthread_mutex_init */
#include <ctype.h> 				/* isdigit, isblank */
#include <stdio.h> 				/* printf, fflush(stdout), sprintf */
#include <stdlib.h> 			/* atoi, malloc, free, realloc */
#include <stdint.h> 			/* uint32_t */
#include <errno.h> 				/* errno */
#include <string.h> 			/* strerror, memset, strncpy, memcpy */
#include <strings.h> 			/* strncasecmp */
#include <unistd.h> 			/* close */
#include <math.h> 				/* log10 */
#include <signal.h> 			/* sigaction */

#include <sys/types.h> 			/* socket, setsockopt, accept, send, recv */
#include <sys/socket.h> 		/* socket, setsockopt, inet_ntoa, accept */
#include <netinet/in.h> 		/* sockaddr_in, inet_ntoa */
#include <arpa/inet.h> 			/* htonl, htons, inet_ntoa */
#include <sys/stat.h> 			/* stat */

#define KEYSIZE 16 				/* The size of the key in Hybi-00 */
#define BUFFERSIZE 8192 		/* Buffer size = 8KB */
#define MAXMESSAGE 1048576 		/* Max size message = 1MB */
#define ORIGIN_REQUIRED 0 		/* If this value is other than 0, client must 
								   supply origin in header */

#endif
