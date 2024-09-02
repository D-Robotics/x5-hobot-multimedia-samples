/***************************************************************************
* COPYRIGHT NOTICE
* Copyright 2019 Horizon Robotics, Inc.
* All rights reserved.
***************************************************************************/
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <hb_mem_mgr.h>
#include <hb_mem_err.h>
#include "sample_common.h"

int sample_share_pool()
{
	int32_t i = 0;
	int32_t ret = 0;
	uint32_t num = 10;
	uint64_t size = 4*1024*1024;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	hb_mem_common_buf_t com_buf[10] = {0, };
	hb_mem_share_pool_t pool = {0, };

	printf("=================================================\n");
	printf("Ready to sample_share_pool\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	ret = hb_mem_share_pool_create(num, size, flags, &pool);
	if (ret != 0) {
		printf("hb_mem_share_pool_create failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	for (i = 0; i < num; i++) {
		ret = hb_mem_share_pool_alloc_buf(pool.fd, &com_buf[i]);
		if (ret != 0) {
			printf("hb_mem_share_pool_alloc_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		ret = hb_mem_share_pool_get_info(pool.fd, &pool);
		if (ret != 0) {
			printf("hb_mem_share_pool_get_info failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		printf("pool.avail_buf_cnt: %d\n", pool.avail_buf_cnt);
	}

	for (i = 0; i < num; i++) {
		ret = hb_mem_share_pool_free_buf((uint64_t)(com_buf[i].virt_addr));
		if (ret != 0) {
			printf("hb_mem_share_pool_free_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}

		ret = hb_mem_share_pool_get_info(pool.fd, &pool);
		if (ret != 0) {
			printf("hb_mem_share_pool_get_info failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		printf("pool.avail_buf_cnt: %d\n", pool.avail_buf_cnt);
	}

	ret = hb_mem_share_pool_destroy(pool.fd);
	if (ret != 0) {
		printf("hb_mem_share_pool_destroy failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	// do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);
	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}
	printf("sample_share_pool done\n");
	printf("=================================================\n");

	return 0;
}

int sample_share_pool_fork_process_scenario()
{
	int32_t ret, i;
	pid_t child;
	uint64_t size = 1024 * 128; // 4k
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	int32_t sd[2];
	int32_t status;

	printf("=================================================\n");
	printf("Ready to sample_share_pool_fork_process_scenario\n");

	socketpair(AF_UNIX, SOCK_STREAM, 0, sd);

	child = fork();
	if (child < 0) {
		printf("[%d:%d] Fail to create child process!\n", getpid(), getppid());
		exit(1);
	} else if (child == 0) {
		hb_mem_common_buf_t recv_buf = {0, };
		hb_mem_common_buf_t out_buf = {0, };
		struct msghdr msg;
		struct iovec io;

		printf("[%d:%d] In child process.\n", getpid(), getppid());

		close(sd[1]);
		memset(&msg, 0x00, sizeof(msg));
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &io;
		msg.msg_iovlen = 1;
		io.iov_base = &recv_buf;
		io.iov_len = sizeof(recv_buf);

		ret = recvmsg(sd[0], &msg, 0);
		printf("[%d:%d] child recv msg 1\n", getpid(), getppid());
		ret = hb_mem_module_open();
		if (ret != 0) {
			printf("hb_mem_module_open failed\n");
			return ret;
		}

		ret = hb_mem_import_com_buf(&recv_buf, &out_buf);
		if (ret != 0) {
			printf("hb_mem_import_com_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}

		ret = sendmsg(sd[0], &msg, 0);
		printf("[%d:%d] child send msg 2\n", getpid(), getppid());

		ret = recvmsg(sd[0], &msg, 0);
		printf("[%d:%d] child recv msg 3\n", getpid(), getppid());

		ret = hb_mem_free_buf(out_buf.fd);
		if (ret != 0) {
			printf("hb_mem_free_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}

		ret = sendmsg(sd[0], &msg, 0);
		printf("[%d:%d] child send msg 4\n", getpid(), getppid());

		ret = hb_mem_module_close();
		if (ret != 0) {
			printf("hb_mem_module_close failed\n");
			return ret;
		}
		printf("[%d:%d] child quit\n", getpid(), getppid());
		close(sd[0]);

		exit(0);
	} else {
		hb_mem_share_pool_t pool = {0, };
		hb_mem_common_buf_t in_buf[5] = {0, };
		hb_mem_common_buf_t recv_buf = {0, };
		struct msghdr msg;
		struct iovec io;

		printf( "[%d:%d] In parent process.\n", getpid(), getppid());
		ret = hb_mem_module_open();
		if (ret != 0) {
			printf("hb_mem_module_open failed\n");
			return ret;
		}

		flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN ;
		ret = hb_mem_share_pool_create(5, size, flags, &pool);
		if (ret != 0) {
			printf("hb_mem_share_pool_create failed\n");
			(void)hb_mem_module_close();
			return ret;
		}

		do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);
		for (i = 0; i < 5; i++) {
			ret = hb_mem_share_pool_alloc_buf(pool.fd, &in_buf[i]);
			if (ret != 0) {
				printf("hb_mem_share_pool_alloc_buf failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			printf("hb_mem_share_pool_alloc_buf %d %p\n", i, in_buf[i].virt_addr);
		}

		close(sd[0]);
		memset(&msg, 0x00, sizeof(msg));
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &io;
		msg.msg_iovlen = 1;

		io.iov_base = &in_buf[0];
		io.iov_len = sizeof(in_buf[0]);

		ret = sendmsg(sd[1], &msg, 0);
		printf("[%d:%d] parent send msg 1\n", getpid(), getppid());

		io.iov_base = &recv_buf;
		io.iov_len = sizeof(recv_buf);
		ret = recvmsg(sd[1], &msg, 0);
		printf("[%d:%d] parent recv msg 2\n", getpid(), getppid());

		for (i = 0; i < 5; i++) {
			printf("free %d %p\n", i, in_buf[i].virt_addr);
			ret = hb_mem_share_pool_free_buf((uint64_t)(in_buf[i].virt_addr));
			if (ret != 0) {
				printf("hb_mem_share_pool_free_buf failed\n");
				(void)hb_mem_module_close();
				return ret;
			}

			ret = hb_mem_share_pool_get_info(pool.fd, &pool);
			if (ret != 0) {
				printf("hb_mem_share_pool_get_info failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			printf("[%d:%d] parent pool.avail_buf_cnt: %d\n", getpid(), getppid(), pool.avail_buf_cnt);
		}

		io.iov_base = &in_buf[0];
		io.iov_len = sizeof(in_buf[0]);
		ret = sendmsg(sd[1], &msg, 0);
		printf("[%d:%d] parent send msg 3\n", getpid(), getppid());

		io.iov_base = &recv_buf;
		io.iov_len = sizeof(recv_buf);
		ret = recvmsg(sd[1], &msg, 0);
		printf("[%d:%d] parent recv msg 4\n", getpid(), getppid());
		ret = hb_mem_share_pool_get_info(pool.fd, &pool);
		if (ret != 0) {
			printf("hb_mem_share_pool_get_info failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		printf("[%d:%d] parent pool.avail_buf_cnt: %d\n", getpid(), getppid(), pool.avail_buf_cnt);

		ret = hb_mem_share_pool_destroy(pool.fd);
		if (ret != 0) {
			printf("hb_mem_share_pool_destroy failed\n");
			(void)hb_mem_module_close();
			return ret;
		}

		ret = hb_mem_module_close();
		if (ret != 0) {
			printf("hb_mem_module_close failed\n");
			return ret;
		}
		waitpid(child, &status, 0);
		close(sd[1]);
		printf("[%d:%d] parent quit\n", getpid(), getppid());
	}
	printf("sample_share_pool_fork_process_scenario done\n");
	printf("=================================================\n");

	return 0;
}