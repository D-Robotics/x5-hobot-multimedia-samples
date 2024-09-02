/***************************************************************************
* COPYRIGHT NOTICE
* Copyright 2024 D-Robotics, Inc.
* All rights reserved.
***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <getopt.h>

#include "sample_common.h"
#define SAMPLE_MODU_LIST_COUNT 12

int sample_mode = 0;

static char *sample_mode_list[] = {
	"Alloc Com Buf",
	"Alloc Com Buf With Cache",
	"Alloc Com Buf With Heapmask",
	"Alloc Graph Buf",
	"Alloc Graph Buf With Heapmask",
	"Share Com Buffer",
	"Share Com Buffer Fork Process Scenario",
	"Share Graph Buffer",
	"Share Graph Buffer Fork Process Scenario",
	"Share Pool Fork Process Scenario",
	"Share Pool",
	"Change Com Graph Buf"
};

static void print_list() {
	int i;
	printf("***************  Sample Mode Lists  ***************\n");
	for(i = 0; i < SAMPLE_MODU_LIST_COUNT; i++){
		printf("index:	%d	sample_mode:	%s\n", i + 1, *(sample_mode_list + i));
	}
	printf("***************************************************\n");
}

static void print_help(const char *prog) {
	printf("Options:\n");
	printf("  -m                     Specify sample use cases \n");
	printf("  -h                     Show this help message\n");
	printf("Usage: %s -m <index>\n", prog);
	print_list();
}

int main(int argc, char *argv[])
{
	int c = 0;
	int opt_index = 0;
	static const struct option long_options[] = {
		{"mode", 1, 0, 'm'},
		{NULL, 0, 0, 0},
	};
	while((c = getopt_long(argc, argv, "m:h", long_options, &opt_index)) != -1) {
		switch (c)
		{
			case 'm':
				sample_mode = atoi(optarg);
				printf("sample_mode = %d\n", sample_mode);
				break;
			case 'h':
			default:
				break;
		}
	}
	switch (sample_mode)
	{
	case 1:
		sample_alloc_com_buf();
		break;
	case 2:
		sample_alloc_com_buf_with_cache();
		break;
	case 3:
		sample_alloc_com_buf_with_heapmask();
		break;
	case 4:
		sample_alloc_graph_buf();
		break;
	case 5:
		sample_alloc_graph_buf_with_heapmask();
		break;
	case 6:
		sample_share_com_buffer();
		break;
	case 7:
		sample_share_com_buffer_fork_process_scenario();
		break;
	case 8:
		sample_share_graph_buffer();
		break;
	case 9:
		sample_share_graph_buffer_fork_process_scenario();
		break;
	case 10:
		sample_share_pool_fork_process_scenario();
		break;
	case 11:
		sample_share_pool();
		break;
	case 12:
		sample_change_com_graph_buf();
		break;
	default:
		print_help(argv[0]);
		break;
	}

	return 0;
}
