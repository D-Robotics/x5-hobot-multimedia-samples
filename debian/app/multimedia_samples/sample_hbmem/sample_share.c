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

#define MAX_PROCESS_INFO 16

static int64_t avail_heap_flag[MAX_HEAP_NUM] = {HB_MEM_USAGE_PRIV_HEAP_DMA,
						HB_MEM_USAGE_PRIV_HEAP_RESERVED,
						HB_MEM_USAGE_PRIV_HEAP_2_RESERVED,
					};

int sample_share_com_buffer()
{
	int32_t ret = 0;
	uint64_t size = 1024 * 64; // 4k
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	hb_mem_common_buf_t in_buf = {0, };
	hb_mem_common_buf_t out_buf = {0, };

	printf("=================================================\n");
	printf("Ready to sample_share_com_buffer\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	ret = hb_mem_alloc_com_buf(size, flags, &in_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_import_com_buf(&in_buf, &out_buf);
	if (ret != 0) {
		printf("hb_mem_import_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("alloc com buf, share_id: %d\n", in_buf.share_id);
	printf("import com buf, share_id: %d\n", out_buf.share_id);
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);

	ret = hb_mem_free_buf(in_buf.fd);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("free com buf\n");
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);
	ret = hb_mem_free_buf(out_buf.fd);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("free import buf\n");
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);

	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}
	printf("sample_share_com_buffer done\n");
	printf("=================================================\n");

	return 0;
}

int sample_share_com_buffer_use_consume_cnt()
{
	int32_t ret = 0, i = 0;
	uint64_t size = 1024 * 64; // 4k
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	int32_t ret_num;
	int32_t pid[MAX_PROCESS_INFO+1];
	hb_mem_common_buf_t in_buf = {0, };
	hb_mem_common_buf_t out_buf = {0, };
	int32_t share_consume_cnt;
	int64_t timeout = 10;

	printf("=================================================\n");
	printf("Ready to sample_share_com_buffer_use_consume_cnt\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	ret = hb_mem_alloc_com_buf(size, flags, &in_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_get_buffer_process_info((uint64_t)in_buf.virt_addr, pid, 1, &ret_num);
	if (ret != 0) {
		printf("hb_mem_get_buffer_process_info failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	for (i = 0; i < ret_num; i++) {
		printf("in_buf used by process[%d]\n", pid[i]);
	}

	ret = hb_mem_import_com_buf(&in_buf, &out_buf);
	if (ret != 0) {
		printf("hb_mem_import_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_inc_com_buf_consume_cnt(&out_buf);
	if (ret != 0) {
		printf("hb_mem_inc_com_buf_consume_cnt failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_get_consume_info_with_vaddr((uint64_t)out_buf.virt_addr, &share_consume_cnt);
	if (ret != 0) {
		printf("hb_mem_get_consume_info_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("share_consume_cnt: %d\n", share_consume_cnt);

	share_consume_cnt = 0;
	ret = hb_mem_wait_consume_status(out_buf.fd, share_consume_cnt, timeout);
	if (ret == HB_MEM_ERR_TIMEOUT) {
		printf("hb_mem_wait_consume_status timeout\n");
	}

	ret = hb_mem_dec_consume_cnt_with_vaddr((uint64_t)out_buf.virt_addr);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	share_consume_cnt = 0;
	ret = hb_mem_wait_consume_status(out_buf.fd, share_consume_cnt, timeout);
	if (ret == HB_MEM_ERR_TIMEOUT) {
		printf("hb_mem_wait_consume_status timeout\n");
	}

	ret = hb_mem_free_buf(in_buf.fd);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	ret = hb_mem_free_buf(out_buf.fd);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}
	printf("sample_share_com_buffer_use_consume_cnt done\n");
	printf("=================================================\n");

	return 0;
}

int sample_share_com_buffer_fork_process_scenario()
{
	int32_t ret = 0, i = 0;
	pid_t child;
	uint64_t size = 1024 * 64; // 4k
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	int64_t tmp_flags = 0;
	int32_t sd[2];
	int32_t status;
	int32_t share_consume_cnt;

	printf("=================================================\n");
	printf("Ready to sample_share_com_buffer_fork_process_scenario\n");

	for (i = 0; i < MAX_HEAP_NUM; i++) {
		tmp_flags = flags | avail_heap_flag[i];
		printf("alloc graph buf form %s\n", get_heap_name(avail_heap_flag[i]));
		ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sd);
		printf("socketpair: %d\n", ret);

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
			printf("child read share buf: %s\n", out_buf.virt_addr);

			ret = hb_mem_inc_com_buf_consume_cnt(&out_buf);
			if (ret != 0) {
				printf("hb_mem_inc_com_buf_consume_cnt failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			ret = hb_mem_get_consume_info(out_buf.fd, &share_consume_cnt);
			if (ret != 0) {
				printf("hb_mem_get_consume_info failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			printf("share_consume_cnt: %d\n", share_consume_cnt);

			strcpy((char*)out_buf.virt_addr, "child test common buf share.");
			printf("child write share buf: %s\n", out_buf.virt_addr);
			ret = sendmsg(sd[0], &msg, 0);
			printf("[%d:%d] child send msg 2\n", getpid(), getppid());

			ret = recvmsg(sd[0], &msg, 0);
			printf("[%d:%d] child recv msg 3\n", getpid(), getppid());

			ret = hb_mem_dec_consume_cnt(out_buf.fd);
			if (ret != 0) {
				printf("hb_mem_dec_consume_cnt failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			ret = hb_mem_get_consume_info(out_buf.fd, &share_consume_cnt);
			if (ret != 0) {
				printf("hb_mem_get_consume_info failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			printf("share_consume_cnt: %d\n", share_consume_cnt);

			ret = hb_mem_free_buf(out_buf.fd);
			if (ret != 0) {
				printf("hb_mem_free_buf failed\n");
				(void)hb_mem_module_close();
				return ret;
			}

			ret = hb_mem_module_close();
			if (ret != 0) {
				printf("hb_mem_module_close failed\n");
				return ret;
			}
			printf("[%d:%d] child quit\n", getpid(), getppid());
			close(sd[0]);
			exit(0);
		} else {
			hb_mem_common_buf_t in_buf = {0, };
			hb_mem_common_buf_t recv_buf = {0, };
			struct msghdr msg;
			struct iovec io;

			close(sd[0]);

			printf( "[%d:%d] In parent process.\n", getpid(), getppid());
			ret = hb_mem_module_open();
			if (ret != 0) {
				printf("hb_mem_module_open failed\n");
				return ret;
			}
			ret = hb_mem_alloc_com_buf(size, tmp_flags, &in_buf);
			if (ret != 0) {
				printf("hb_mem_alloc_com_buf failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			//
			strcpy((char*)in_buf.virt_addr, "parent test common buf share.");
			printf("parent write share buf: %s\n", in_buf.virt_addr);

			ret = hb_mem_get_consume_info(in_buf.fd, &share_consume_cnt);
			if (ret != 0) {
				printf("hb_mem_get_consume_info failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			printf("share_consume_cnt: %d\n", share_consume_cnt);

			memset(&msg, 0x00, sizeof(msg));
			msg.msg_name = NULL;
			msg.msg_namelen = 0;
			msg.msg_iov = &io;
			msg.msg_iovlen = 1;
			io.iov_base = &in_buf;
			io.iov_len = sizeof(in_buf);
			ret = sendmsg(sd[1], &msg, 0);
			printf("[%d:%d] parent send msg 1\n", getpid(), getppid());

			io.iov_base = &recv_buf;
			io.iov_len = sizeof(recv_buf);
			ret = recvmsg(sd[1], &msg, 0);
			printf("[%d:%d] parent recv msg 2\n", getpid(), getppid());
			printf("parent read share buf: %s\n", in_buf.virt_addr);

			ret = hb_mem_free_buf(in_buf.fd);
			if (ret != 0) {
				printf("hb_mem_free_buf failed\n");
				(void)hb_mem_module_close();
				return ret;
			}

			io.iov_base = &in_buf;
			io.iov_len = sizeof(in_buf);
			ret = sendmsg(sd[1], &msg, 0);
			printf("[%d:%d] parent send msg 3\n", getpid(), getppid());

			ret = hb_mem_module_close();
			if (ret != 0) {
				printf("hb_mem_module_close failed\n");
				return ret;
			}

			printf("[%d:%d] parent quit\n", getpid(), getppid());
			waitpid(child, &status, 0);
			close(sd[1]);
		}
	}
	printf("sample_share_com_buffer_fork_process_scenario done\n");
	printf("=================================================\n");

	return 0;
}

int sample_share_graph_buffer()
{
	int32_t ret = 0;
	int32_t w = 1280, h = 720, format = MEM_PIX_FMT_NV12, stride = 0, vstride = 0;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	hb_mem_graphic_buf_t in_buf = {0, };
	hb_mem_graphic_buf_t out_buf = {0, };

	printf("=================================================\n");
	printf("Ready to sample_share_graph_buffer\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	ret = hb_mem_alloc_graph_buf(w, h, format, flags, stride, vstride, &in_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_graph_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_import_graph_buf(&in_buf, &out_buf);
	if (ret != 0) {
		printf("hb_mem_import_graph_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("alloc graph buf, share_id0: %d, share_id1: %d, share_id2: %d\n",
			in_buf.share_id[0], in_buf.share_id[1], in_buf.share_id[1]);
	printf("import graph buf, share_id0: %d, share_id1: %d, share_id2: %d\n",
			out_buf.share_id[0], out_buf.share_id[1], out_buf.share_id[1]);
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);

	ret = hb_mem_free_buf(in_buf.fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("free graph buf\n");
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);
	ret = hb_mem_free_buf(out_buf.fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("free import buf\n");
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);

	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}
	printf("sample_share_graph_buffer done\n");
	printf("=================================================\n");

	return 0;
}

int sample_share_graph_buffer_use_consume_cnt()
{
	int32_t ret = 0, i = 0;
	int32_t ret_num;
	int32_t pid[MAX_PROCESS_INFO + 1];
	int32_t share_consume_cnt;
	int64_t timeout = 10;
	int32_t w = 1280, h = 720, format = MEM_PIX_FMT_NV12, stride = 0, vstride = 0;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	hb_mem_graphic_buf_t in_buf = {0, };
	hb_mem_graphic_buf_t out_buf = {0, };

	printf("=================================================\n");
	printf("Ready to sample_share_graph_buffer_use_consume_cnt\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	ret = hb_mem_alloc_graph_buf(w, h, format, flags, stride, vstride, &in_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_graph_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_get_buffer_process_info((uint64_t)in_buf.virt_addr[0], pid, 1, &ret_num);
	if (ret != 0) {
		printf("hb_mem_get_buffer_process_info failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	for (i = 0; i < ret_num; i++) {
		printf("in_buf used by process[%d]\n", pid[i]);
	}

	ret = hb_mem_import_graph_buf(&in_buf, &out_buf);
	if (ret != 0) {
		printf("hb_mem_import_graph_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_inc_graph_buf_consume_cnt(&out_buf);
	if (ret != 0) {
		printf("hb_mem_inc_graph_buf_consume_cnt failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_get_consume_info_with_vaddr((uint64_t)out_buf.virt_addr[0], &share_consume_cnt);
	if (ret != 0) {
		printf("hb_mem_get_consume_info_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("share_consume_cnt: %d\n", share_consume_cnt);

	share_consume_cnt = 0;
	ret = hb_mem_wait_consume_status(out_buf.fd[0], share_consume_cnt, timeout);
	if (ret == HB_MEM_ERR_TIMEOUT) {
		printf("hb_mem_wait_consume_status timeout\n");
	}

	ret = hb_mem_dec_consume_cnt_with_vaddr((uint64_t)out_buf.virt_addr[0]);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	share_consume_cnt = 0;
	ret = hb_mem_wait_consume_status(out_buf.fd[0], share_consume_cnt, timeout);
	if (ret == HB_MEM_ERR_TIMEOUT) {
		printf("hb_mem_wait_consume_status timeout\n");
	}

	ret = hb_mem_free_buf(in_buf.fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	ret = hb_mem_free_buf(out_buf.fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}
	printf("sample_share_graph_buffer_use_consume_cnt done\n");
	printf("=================================================\n");

	return 0;
}

int sample_share_graph_buffer_fork_process_scenario()
{
	int32_t ret = 0, i = 0;
	pid_t child;
	int32_t w = 1280, h = 720, format = MEM_PIX_FMT_NV12, stride = 0, vstride = 0;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	int64_t tmp_flags = 0;
	int32_t sd[2];
	int32_t status;
	int32_t share_consume_cnt;

	printf("=================================================\n");
	printf("Ready to sample_share_graph_buffer_fork_process_scenario\n");

	for (i = 0; i < MAX_HEAP_NUM; i++) {
		tmp_flags = flags | avail_heap_flag[i];
		printf("alloc graph buf form %s\n", get_heap_name(avail_heap_flag[i]));
		ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sd);
		printf("socketpair: %d\n", ret);

		child = fork();
		if (child < 0) {
			printf("[%d:%d] Fail to create child process!\n", getpid(), getppid());
			exit(1);
		} else if (child == 0) {
			hb_mem_graphic_buf_t recv_buf = {0, };
			hb_mem_graphic_buf_t out_buf = {0, };
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

			ret = hb_mem_import_graph_buf(&recv_buf, &out_buf);
			if (ret != 0) {
				printf("hb_mem_import_graph_buf failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			printf("child read share buf: %s\n", out_buf.virt_addr[0]);

			ret = hb_mem_inc_graph_buf_consume_cnt(&out_buf);
			if (ret != 0) {
				printf("hb_mem_inc_graph_buf_consume_cnt failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			ret = hb_mem_get_consume_info(out_buf.fd[0], &share_consume_cnt);
			if (ret != 0) {
				printf("hb_mem_get_consume_info failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			printf("share_consume_cnt: %d\n", share_consume_cnt);

			strcpy((char*)out_buf.virt_addr[0], "child test common buf share.");
			printf("child write share buf: %s\n", out_buf.virt_addr[0]);
			ret = sendmsg(sd[0], &msg, 0);
			printf("[%d:%d] child send msg 2\n", getpid(), getppid());

			ret = recvmsg(sd[0], &msg, 0);
			printf("[%d:%d] child recv msg 3\n", getpid(), getppid());

			ret = hb_mem_dec_consume_cnt(out_buf.fd[0]);
			if (ret != 0) {
				printf("hb_mem_dec_consume_cnt failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			ret = hb_mem_get_consume_info(out_buf.fd[0], &share_consume_cnt);
			if (ret != 0) {
				printf("hb_mem_get_consume_info failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			printf("share_consume_cnt: %d\n", share_consume_cnt);

			ret = hb_mem_free_buf(out_buf.fd[0]);
			if (ret != 0) {
				printf("hb_mem_free_buf failed\n");
				(void)hb_mem_module_close();
				return ret;
			}

			ret = hb_mem_module_close();
			if (ret != 0) {
				printf("hb_mem_module_close failed\n");
				return ret;
			}
			printf("[%d:%d] child quit\n", getpid(), getppid());
			close(sd[0]);
			exit(0);
		} else {
			hb_mem_graphic_buf_t in_buf = {0, };
			hb_mem_graphic_buf_t recv_buf = {0, };
			struct msghdr msg;
			struct iovec io;

			close(sd[0]);

			printf( "[%d:%d] In parent process.\n", getpid(), getppid());
			ret = hb_mem_module_open();
			if (ret != 0) {
				printf("hb_mem_module_open failed\n");
				return ret;
			}
			ret = hb_mem_alloc_graph_buf(w, h, format, tmp_flags, stride, vstride, &in_buf);
			if (ret != 0) {
				printf("hb_mem_alloc_graph_buf failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			//
			strcpy((char*)in_buf.virt_addr[0], "parent test graph buf share.");
			printf("parent write share buf: %s\n", in_buf.virt_addr[0]);

			ret = hb_mem_get_consume_info(in_buf.fd[0], &share_consume_cnt);
			if (ret != 0) {
				printf("hb_mem_get_consume_info failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
			printf("share_consume_cnt: %d\n", share_consume_cnt);

			memset(&msg, 0x00, sizeof(msg));
			msg.msg_name = NULL;
			msg.msg_namelen = 0;
			msg.msg_iov = &io;
			msg.msg_iovlen = 1;
			io.iov_base = &in_buf;
			io.iov_len = sizeof(in_buf);
			ret = sendmsg(sd[1], &msg, 0);
			printf("[%d:%d] parent send msg 1\n", getpid(), getppid());

			io.iov_base = &recv_buf;
			io.iov_len = sizeof(recv_buf);
			ret = recvmsg(sd[1], &msg, 0);
			printf("[%d:%d] parent recv msg 2\n", getpid(), getppid());
			printf("parent read share buf: %s\n", in_buf.virt_addr[0]);

			ret = hb_mem_free_buf(in_buf.fd[0]);
			if (ret != 0) {
				printf("hb_mem_free_buf failed\n");
				(void)hb_mem_module_close();
				return ret;
			}

			io.iov_base = &in_buf;
			io.iov_len = sizeof(in_buf);
			ret = sendmsg(sd[1], &msg, 0);
			printf("[%d:%d] parent send msg 3\n", getpid(), getppid());

			ret = hb_mem_module_close();
			if (ret != 0) {
				printf("hb_mem_module_close failed\n");
				return ret;
			}

			printf("[%d:%d] parent quit\n", getpid(), getppid());
			waitpid(child, &status, 0);
			close(sd[1]);
		}
	}
	printf("sample_share_graph_buffer_fork_process_scenario done\n");
	printf("=================================================\n");

	return 0;
}

int sample_share_graph_buffer_group()
{
	int32_t ret = 0, i = 0;
	uint32_t bitmap = 0xFF;
	int32_t w[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t h[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t format[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t stride[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t vstride[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int64_t flags[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	hb_mem_graphic_buf_group_t in_buf = {0, };
	hb_mem_graphic_buf_group_t out_buf = {0, };

	printf("=================================================\n");
	printf("Ready to sample_share_graph_buffer_group\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	for (i = 0; i < HB_MEM_MAXIMUM_GRAPH_BUF; i++) {
		if (bitmap & (1u << i)) {
			w[i] = 1280;
			h[i] = 720;
			format[i] = MEM_PIX_FMT_NV12;
			flags[i] = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;;
			stride[i] = 0;
			vstride[i] = 0;
		}
	}

	ret = hb_mem_alloc_graph_buf_group(w, h, format, flags, stride, vstride, &in_buf, bitmap);;
	if (ret != 0) {
		printf("hb_mem_alloc_graph_buf_group failed\n");
		return ret;
	}

	ret = hb_mem_import_graph_buf_group(&in_buf, &out_buf);
	if (ret != 0) {
		printf("hb_mem_import_graph_buf_group failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	do_sys_command(flags[0] & HB_MEM_USAGE_PRIV_MASK);

	ret = hb_mem_free_buf(in_buf.graph_group[0].fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("free graph buf group\n");
	do_sys_command(flags[0] & HB_MEM_USAGE_PRIV_MASK);
	ret = hb_mem_free_buf(in_buf.graph_group[0].fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("free import buf\n");
	do_sys_command(flags[0] & HB_MEM_USAGE_PRIV_MASK);

	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}
	printf("sample_share_graph_buffer_group done\n");
	printf("=================================================\n");

	return 0;
}

int sample_share_graph_buffer_group_use_consume_cnt()
{
	int32_t ret = 0, i = 0;
	int32_t ret_num;
	int32_t pid[MAX_PROCESS_INFO + 1];
	int32_t share_consume_cnt;
	int64_t timeout = 10;
	uint32_t bitmap = 0xFF;
	int32_t w[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t h[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t format[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t stride[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t vstride[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int64_t flags[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	hb_mem_graphic_buf_group_t in_buf = {0, };
	hb_mem_graphic_buf_group_t out_buf = {0, };

	printf("=================================================\n");
	printf("Ready to sample_share_graph_buffer_group_use_consume_cnt\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	for (i = 0; i < HB_MEM_MAXIMUM_GRAPH_BUF; i++) {
		if (bitmap & (1u << i)) {
			w[i] = 1280;
			h[i] = 720;
			format[i] = MEM_PIX_FMT_NV12;
			flags[i] = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;;
			stride[i] = 0;
			vstride[i] = 0;
		}
	}

	ret = hb_mem_alloc_graph_buf_group(w, h, format, flags, stride, vstride, &in_buf, bitmap);;
	if (ret != 0) {
		printf("hb_mem_alloc_graph_buf_group failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_get_buffer_process_info((uint64_t)in_buf.graph_group[0].virt_addr[0], pid, 1, &ret_num);
	if (ret != 0) {
		printf("hb_mem_get_buffer_process_info failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	for (i = 0; i < ret_num; i++) {
		printf("in_buf used by process[%d]\n", pid[i]);
	}

	ret = hb_mem_import_graph_buf_group(&in_buf, &out_buf);
	if (ret != 0) {
		printf("hb_mem_import_graph_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_inc_graph_buf_group_consume_cnt(&out_buf);
	if (ret != 0) {
		printf("hb_mem_inc_graph_buf_consume_cnt failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_get_consume_info_with_vaddr((uint64_t)out_buf.graph_group[0].virt_addr[0], &share_consume_cnt);
	if (ret != 0) {
		printf("hb_mem_get_consume_info_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("share_consume_cnt: %d\n", share_consume_cnt);

	share_consume_cnt = 0;
	ret = hb_mem_wait_consume_status(out_buf.graph_group[0].fd[0], share_consume_cnt, timeout);
	if (ret == HB_MEM_ERR_TIMEOUT) {
		printf("hb_mem_wait_consume_status timeout\n");
	}

	ret = hb_mem_dec_consume_cnt_with_vaddr((uint64_t)out_buf.graph_group[0].virt_addr[0]);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	share_consume_cnt = 0;
	ret = hb_mem_wait_consume_status(out_buf.graph_group[0].fd[0], share_consume_cnt, timeout);
	if (ret == HB_MEM_ERR_TIMEOUT) {
		printf("hb_mem_wait_consume_status timeout\n");
	}

	ret = hb_mem_free_buf(in_buf.graph_group[0].fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	ret = hb_mem_free_buf(out_buf.graph_group[0].fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}
	printf("sample_share_graph_buffer_group_use_consume_cnt done\n");
	printf("=================================================\n");

	return 0;
}

int sample_share_graph_buffer_group_fork_process_scenario()
{
	int32_t ret, i;
	pid_t child;
	uint32_t bitmap = 0xFF;
	int32_t w[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t h[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t format[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t stride[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t vstride[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int64_t flags[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t sd[2];
	int32_t status;
	int32_t share_consume_cnt;

	printf("=================================================\n");
	printf("Ready to sample_share_graph_buffer_group_fork_process_scenario\n");

	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sd);
	printf("socketpair: %d\n", ret);

	for (i = 0; i < HB_MEM_MAXIMUM_GRAPH_BUF; i++) {
		if (bitmap & (1u << i)) {
			w[i] = 1280;
			h[i] = 720;
			format[i] = MEM_PIX_FMT_NV12;
			flags[i] = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;;
			stride[i] = 0;
			vstride[i] = 0;
		}
	}

	child = fork();
	if (child < 0) {
		printf("[%d:%d] Fail to create child process!\n", getpid(), getppid());
		exit(1);
	} else if (child == 0) {
		hb_mem_graphic_buf_group_t recv_buf = {0, };
		hb_mem_graphic_buf_group_t out_buf = {0, };
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

		ret = hb_mem_import_graph_buf_group(&recv_buf, &out_buf);
		if (ret != 0) {
			printf("hb_mem_import_graph_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		printf("child read share buf: %s\n", out_buf.graph_group[0].virt_addr[0]);

		ret = hb_mem_inc_graph_buf_group_consume_cnt(&out_buf);
		if (ret != 0) {
			printf("hb_mem_inc_graph_buf_consume_cnt failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		ret = hb_mem_get_consume_info(out_buf.graph_group[0].fd[0], &share_consume_cnt);
		if (ret != 0) {
			printf("hb_mem_get_consume_info failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		printf("share_consume_cnt: %d\n", share_consume_cnt);

		strcpy((char*)out_buf.graph_group[0].virt_addr[0], "child test common buf share.");
		printf("child write share buf: %s\n", out_buf.graph_group[0].virt_addr[0]);
		ret = sendmsg(sd[0], &msg, 0);
		printf("[%d:%d] child send msg 2\n", getpid(), getppid());

		ret = recvmsg(sd[0], &msg, 0);
		printf("[%d:%d] child recv msg 3\n", getpid(), getppid());

		ret = hb_mem_dec_consume_cnt(out_buf.graph_group[0].fd[0]);
		if (ret != 0) {
			printf("hb_mem_dec_consume_cnt failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		ret = hb_mem_get_consume_info(out_buf.graph_group[0].fd[0], &share_consume_cnt);
		if (ret != 0) {
			printf("hb_mem_get_consume_info failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		printf("share_consume_cnt: %d\n", share_consume_cnt);

		ret = hb_mem_free_buf(out_buf.graph_group[0].fd[0]);
		if (ret != 0) {
			printf("hb_mem_free_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}

		ret = hb_mem_module_close();
		if (ret != 0) {
			printf("hb_mem_module_close failed\n");
			return ret;
		}
		printf("[%d:%d] child quit\n", getpid(), getppid());
		close(sd[0]);
		exit(0);
	} else {
		hb_mem_graphic_buf_group_t in_buf = {0, };
		hb_mem_graphic_buf_group_t recv_buf = {0, };
		struct msghdr msg;
		struct iovec io;

		close(sd[0]);

		printf( "[%d:%d] In parent process.\n", getpid(), getppid());
		ret = hb_mem_module_open();
		if (ret != 0) {
			printf("hb_mem_module_open failed\n");
			return ret;
		}
		ret = hb_mem_alloc_graph_buf_group(w, h, format, flags, stride, vstride, &in_buf, bitmap);;
		if (ret != 0) {
			printf("hb_mem_alloc_graph_buf_group failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		//
		strcpy((char*)in_buf.graph_group[0].virt_addr[0], "parent test graph buf share.");
		printf("parent write share buf: %s\n", in_buf.graph_group[0].virt_addr[0]);

		ret = hb_mem_get_consume_info(in_buf.graph_group[0].fd[0], &share_consume_cnt);
		if (ret != 0) {
			printf("hb_mem_get_consume_info failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		printf("share_consume_cnt: %d\n", share_consume_cnt);

		memset(&msg, 0x00, sizeof(msg));
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &io;
		msg.msg_iovlen = 1;
		io.iov_base = &in_buf;
		io.iov_len = sizeof(in_buf);
		ret = sendmsg(sd[1], &msg, 0);
		printf("[%d:%d] parent send msg 1\n", getpid(), getppid());

		io.iov_base = &recv_buf;
		io.iov_len = sizeof(recv_buf);
		ret = recvmsg(sd[1], &msg, 0);
		printf("[%d:%d] parent recv msg 2\n", getpid(), getppid());
		printf("parent read share buf: %s\n", in_buf.graph_group[0].virt_addr[0]);

		ret = hb_mem_free_buf(in_buf.graph_group[0].fd[0]);
		if (ret != 0) {
			printf("hb_mem_free_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}

		io.iov_base = &in_buf;
		io.iov_len = sizeof(in_buf);
		ret = sendmsg(sd[1], &msg, 0);
		printf("[%d:%d] parent send msg 3\n", getpid(), getppid());

		ret = hb_mem_module_close();
		if (ret != 0) {
			printf("hb_mem_module_close failed\n");
			return ret;
		}

		printf("[%d:%d] parent quit\n", getpid(), getppid());
		waitpid(child, &status, 0);
		close(sd[1]);
	}
	printf("sample_share_graph_buffer_group_fork_process_scenario done\n");
	printf("=================================================\n");

	return 0;
}