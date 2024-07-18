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
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

#include "Handshake.h"
#include "Errors.h"
#include "handle_user_message.h"

#include "WebsocketWrap.h"

#define PORT 4567

static ws_wrap_t *g_ws_instance = NULL;

ws_wrap_t *ws_wrap_instace()
{
	if (g_ws_instance == NULL)
	{
		g_ws_instance = malloc(sizeof(ws_wrap_t));
		if (g_ws_instance == NULL)
			return NULL;
		/**
		 * Creating new lists, l is supposed to contain the connected users.
		 */
		g_ws_instance->m_list = list_new();
		g_ws_instance->m_port = PORT; // 默认4567端口
	}

	return g_ws_instance;
}

void ws_wrap_destory(ws_wrap_t *instance)
{
	if (instance->m_list)
	{
		list_remove_all(instance->m_list);
		list_free(instance->m_list);
		instance->m_list = NULL;
	}
	if (instance)
		free(instance);
}

/**
 * Handler to call when CTRL+C is typed. This function shuts down the server
 * in a safe way.
 */
void sigint_handler(int sig)
{
	if (sig == SIGINT || sig == SIGSEGV)
	{
		if (g_ws_instance->m_list != NULL)
		{
			list_free(g_ws_instance->m_list);
			g_ws_instance->m_list = NULL;
		}
		(void)signal(sig, SIG_DFL);
		exit(0);
	}
	else if (sig == SIGPIPE)
	{
		(void)signal(sig, SIG_IGN);
	}
}

/**
 * Shuts down a client in a safe way. This is only used for Hybi-00.
 */
void cleanup_client(void *args)
{
	ws_client *n = args;
	if (n != NULL)
	{
		printf("Shutting client down..\n\n> ");
		fflush(stdout);
		list_remove(g_ws_instance->m_list, n);
	}
}

int ws_send_message(const char *message, uint64_t length)
{
	ws_connection_close status;
	ws_message *m = message_new();
	m->len = length;

	char *temp = malloc(sizeof(char) * (m->len + 1));
	if (temp == NULL)
	{
		return -1;
	}
	memset(temp, '\0', (m->len + 1));
	memcpy(temp, message, m->len);
	m->msg = temp;
	temp = NULL;

	if ((status = encodeMessage(m)) != CONTINUE)
	{
		message_free(m);
		free(m);
		return -1;
	}

	list_multicast_all(g_ws_instance->m_list, m);
	message_free(m);
	free(m);
	return 0;
}

int ws_send_nalu_to_wfs(ws_client *n, uint32_t header_info, uint64_t timestamp, unsigned char *message, uint64_t length)
{
	ws_connection_close status;
	ws_message *m = message_new();
	m->len = length + sizeof(header_info) + sizeof(timestamp);

	char *temp = malloc(sizeof(char) * (m->len + 1));
	if (temp == NULL)
	{
		return -1;
	}
	memset(temp, '\0', (m->len + 1));

	// 将 header_info 写入 temp
	memcpy(temp, &header_info, sizeof(header_info));
	// 将 timestamp 写入 temp
	memcpy(temp + sizeof(header_info), &timestamp, sizeof(timestamp));
	// 将消息数据复制到 temp 的后面
	memcpy(temp + sizeof(header_info) + sizeof(timestamp), message, length);

	m->msg = temp;
	temp = NULL;

	if ((status = encodeBinary(m)) != CONTINUE)
	{
		message_free(m);
		free(m);
		return -1;
	}

	list_multicast_one(g_ws_instance->m_list, n, m);
	message_free(m);
	free(m);
	return 0;
}

int ws_send_binary(ws_client *n, unsigned char *message, uint64_t length)
{
	ws_connection_close status;
	ws_message *m = message_new();
	m->len = length;

	char *temp = malloc(sizeof(char) * (m->len + 1));
	if (temp == NULL)
	{
		return -1;
	}
	memset(temp, '\0', (m->len + 1));
	memcpy(temp, message, m->len);
	m->msg = temp;
	temp = NULL;

	if ((status = encodeBinary(m)) != CONTINUE)
	{
		message_free(m);
		free(m);
		return -1;
	}

	list_multicast_one(g_ws_instance->m_list, n, m);
	message_free(m);
	free(m);
	return 0;
}

void *handleClient(void *args)
{
	pthread_detach(pthread_self());
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	pthread_cleanup_push(&cleanup_client, args);

	int buffer_length = 0, string_length = 1, reads = 1;

	ws_client *n = args;
	n->thread_id = pthread_self();

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	char buffer[BUFFERSIZE];
	n->string = (char *)malloc(sizeof(char));

	if (n->string == NULL)
	{
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		handshake_error("Couldn't allocate memory.", ERROR_INTERNAL, n);
		pthread_exit((void *)EXIT_FAILURE);
	}

	prctl(PR_SET_NAME, "websocket_server");

	printf("Client connected with the following information:\n"
		   "\tSocket: %d\n"
		   "\tAddress: %s\n\n",
		   n->socket_id, (char *)n->client_ip);
	printf("Checking whether client is valid ...\n\n");
	fflush(stdout);

	/**
	 * Getting headers and doing reallocation if headers is bigger than our
	 * allocated memory.
	 */
	do
	{
		memset(buffer, '\0', BUFFERSIZE);
		if ((buffer_length = recv(n->socket_id, buffer, BUFFERSIZE, 0)) <= 0)
		{
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
			handshake_error("Didn't receive any headers from the client.",
							ERROR_BAD, n);
			pthread_exit((void *)EXIT_FAILURE);
		}

		if (reads == 1 && strlen(buffer) < 14)
		{
			handshake_error("SSL request is not supported yet.",
							ERROR_NOT_IMPL, n);
			pthread_exit((void *)EXIT_FAILURE);
		}

		string_length += buffer_length;

		char *tmp = realloc(n->string, string_length);
		if (tmp == NULL)
		{
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
			handshake_error("Couldn't reallocate memory.", ERROR_INTERNAL, n);
			pthread_exit((void *)EXIT_FAILURE);
		}
		n->string = tmp;
		tmp = NULL;

		memset(n->string + (string_length - buffer_length - 1), '\0',
			   buffer_length + 1);
		memcpy(n->string + (string_length - buffer_length - 1), buffer,
			   buffer_length);
		reads++;
	} while (strncmp("\r\n\r\n", n->string + (string_length - 5), 4) != 0 && strncmp("\n\n", n->string + (string_length - 3), 2) != 0 && strncmp("\r\n\r\n", n->string + (string_length - 8 - 5), 4) != 0 && strncmp("\n\n", n->string + (string_length - 8 - 3), 2) != 0);

	printf("User connected with the following headers:\n%s\n\n", n->string);
	fflush(stdout);

	ws_header *h = header_new();

	if (h == NULL)
	{
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		handshake_error("Couldn't allocate memory.", ERROR_INTERNAL, n);
		pthread_exit((void *)EXIT_FAILURE);
	}

	n->headers = h;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (parseHeaders(n->string, n) < 0)
	{
		pthread_exit((void *)EXIT_FAILURE);
	}

	if (sendHandshake(n) < 0 && n->headers->type != UNKNOWN)
	{
		pthread_exit((void *)EXIT_FAILURE);
	}

	list_add(g_ws_instance->m_list, n);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	printf("Client has been validated and is now connected\n\n");
	/*printf("> ");*/
	/*fflush(stdout);*/

	uint64_t next_len = 0;
	char next[BUFFERSIZE];
	memset(next, '\0', BUFFERSIZE);

	while (1)
	{
		ws_connection_close ret = communicate(n, next, next_len);
		if (ret != CONTINUE)
		{
			printf("communicate disconnected: %d\n", ret);
			break;
		}

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if (n->headers->protocol == WS_CHAT)
		{
			list_multicast(g_ws_instance->m_list, n);
		}
		else if (n->headers->protocol == WS_ECHO)
		{
			list_multicast_one(g_ws_instance->m_list, n, n->message);
		}
#if 0
		} else {
			list_multicast_one(g_ws_instance->m_list, n, n->message);
		}
#endif
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		if (n->message != NULL)
		{
			handle_user_msg(g_ws_instance->m_list, n, n->message->msg);
			memset(next, '\0', BUFFERSIZE);
			memcpy(next, n->message->next, n->message->next_len);
			next_len = n->message->next_len;
			message_free(n->message);
			free(n->message);
			n->message = NULL;
		}
	}

	// 如果推流线程存在就退出
	if (n->stream_thread.pThread_Id != 0)
		mThreadStop(&n->stream_thread);

	printf("Shutting client down..\n\n");

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (g_ws_instance->m_list != NULL && n != NULL)
	{
		list_remove(g_ws_instance->m_list, n);
	}
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	pthread_cleanup_pop(0);
	pthread_exit((void *)EXIT_SUCCESS);
}

static void *_ws_wrap_start(void *ptr)
{
	tsThread *privThread = (tsThread *)ptr;
	int server_socket, client_socket, on = 1;
	ws_wrap_t *ws_wrap = (ws_wrap_t *)privThread->pvThreadData;
	int port = ws_wrap->m_port;

	mThreadSetName(privThread, __func__);

	struct sockaddr_in server_addr, client_addr;
	socklen_t client_length;
	pthread_t pthread_id;
	pthread_attr_t pthread_attr;

	printf("Server: \t\tStarted\n");
	fflush(stdout);

	/**
	 * Assigning port value.
	 */
	printf("Port: \t\t\t%d\n", port);
	fflush(stdout);

	/**
	 * Opening server socket.
	 */
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		server_error(strerror(errno), server_socket, ws_wrap->m_list);
		goto exit;
	}

	printf("Socket: \t\tInitialized\n");
	fflush(stdout);

	/**
	 * Allow reuse of address, when the server shuts down.
	 */
	if ((setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on,
					sizeof(on))) < 0)
	{
		server_error(strerror(errno), server_socket, ws_wrap->m_list);
		goto exit;
	}

	printf("Reuse Port %d: \tEnabled\n", port);
	fflush(stdout);

	memset((char *)&server_addr, '\0', sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	printf("Ip Address: \t\t%s\n", inet_ntoa(server_addr.sin_addr));
	fflush(stdout);

	/**
	 * Bind address.
	 */
	if ((bind(server_socket, (struct sockaddr *)&server_addr,
			  sizeof(server_addr))) < 0)
	{
		server_error(strerror(errno), server_socket, ws_wrap->m_list);
		goto exit;
	}

	printf("Binding: \t\tSuccess\n");
	fflush(stdout);

	/**
	 * Listen on the server socket for connections
	 */
	if ((listen(server_socket, 10)) < 0)
	{
		server_error(strerror(errno), server_socket, ws_wrap->m_list);
		goto exit;
	}

	printf("Listen: \t\tSuccess\n\n");
	fflush(stdout);

	/**
	 * Attributes for the threads we will create when a new client connects.
	 */
	pthread_attr_init(&pthread_attr);
	pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&pthread_attr, 524288);

	printf("Server is now waiting for clients to connect ...\n\n");
	fflush(stdout);

	/**
	 * Do not wait for the thread to terminate.
	 */
	/*pthread_detach(pthread_id);*/

	while (privThread->eState == E_THREAD_RUNNING)
	{
		client_length = sizeof(client_addr);

		/**
		 * If a client connects, we observe it here.
		 */
		if ((client_socket = accept(server_socket,
									(struct sockaddr *)&client_addr,
									&client_length)) < 0)
		{
			server_error(strerror(errno), server_socket, ws_wrap->m_list);
			break;
		}

		/**
		 * Save some information about the client, which we will
		 * later use to identify him with.
		 */
		char *temp = (char *)inet_ntoa(client_addr.sin_addr);
		char *addr = (char *)malloc(sizeof(char) * (strlen(temp) + 1));
		if (addr == NULL)
		{
			server_error(strerror(errno), server_socket, ws_wrap->m_list);
			break;
		}
		memset(addr, '\0', strlen(temp) + 1);
		memcpy(addr, temp, strlen(temp));

		ws_client *n = client_new(client_socket, addr);
		if(n == NULL){
			printf("client_new failed so exit(-1) .\n");
			exit(-1);
		}
		/**
		 * Create client thread, which will take care of handshake and all
		 * communication with the client.
		 */
		if ((pthread_create(&pthread_id, &pthread_attr, handleClient,
							(void *)n)) < 0)
		{
			server_error(strerror(errno), server_socket, ws_wrap->m_list);
			printf("error pthread create failed\n");
			exit(-1);
			break;
		}

		pthread_detach(pthread_id);
	}
	printf("Server is exit ... (websocket)\n\n");
	fflush(stdout);

	close(server_socket);
	pthread_attr_destroy(&pthread_attr);
exit:
	mThreadFinish(privThread);
	return NULL;
}

int ws_wrap_start(int port_num)
{
	g_ws_instance->ws_server_thread.pvThreadData = (void *)g_ws_instance;
	mThreadStart(_ws_wrap_start, &g_ws_instance->ws_server_thread, E_THREAD_JOINABLE);
	return 0;
}

int ws_wrap_stop(void)
{
	mThreadStop(&g_ws_instance->ws_server_thread);
	return 0;
}
