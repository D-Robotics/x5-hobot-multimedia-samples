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

#ifndef _WEBSOCKET_WRAP_H
#define _WEBSOCKET_WRAP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "Communicate.h"
#include "utils/mthread.h"

typedef struct {
	ws_list *m_list;
	int m_port;
	tsThread ws_server_thread;
} ws_wrap_t;

ws_wrap_t *ws_wrap_instace();
void ws_wrap_destory(ws_wrap_t *instance);
int ws_wrap_start(int port_num);
int ws_send_message(const char *message, uint64_t length);
int ws_send_binary(ws_client *n, unsigned char *message, uint64_t length);
int ws_send_nalu_to_wfs(ws_client *n, uint32_t header_info,
		uint64_t timestamp, unsigned char *message, uint64_t length);

#ifdef __cplusplus
}
#endif
#endif // _WEBSOCKET_WRAP_H