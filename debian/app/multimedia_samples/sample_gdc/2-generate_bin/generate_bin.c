/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

#include "hbn_api.h"
#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"

int32_t gdc_gen_bin_and_config(char *file_name, const char *json);

static void print_help() {
	printf("genereate_bin [-c json_config_file] [-o output_file]\n");
}


int main(int argc, char** argv) {
	int32_t ret = 0;
	int c;
	char* config_file = "./gdc_bin_custom_config.json";
	char* gdc_bin_file = "./gdc.bin";

	while((c = getopt(argc, argv, "c:o:h")) != -1) {
		switch (c)
		{
		case 'c':
			config_file = optarg;
			break;
		case 'o':
			gdc_bin_file = optarg;
			break;
		case 'h':

		default:
			print_help();
			return 0;
		}
	}

	printf("Gdc bin custom config: %s\n", config_file);
	printf("Generate gdc bin file: %s\n", gdc_bin_file);
	ret = gdc_gen_bin_and_config(gdc_bin_file, config_file);
	if (ret != 0) {
		printf("Generate bin file failed\n");
	}

	return 0;
}

int32_t gdc_gen_bin_and_config(char *file_name, const char *json)
{
	int32_t ret = 0;
	uint32_t *cfg_buf = NULL;
	uint64_t config_size = 0;
	FILE *fp;
	int n;

	if (file_name == NULL) {
		printf("file_name is NULL\n");
		return -1;
	}
	if (json == NULL) {
		printf("json is NULL\n");
		return -1;
	}
	//generate gdc bin
	ret = hbn_gen_gdc_bin_json(json, NULL, &cfg_buf, &config_size);
	if (ret != 0) {
		printf("hbn_gen_gdc_bin_json failed, ret = %d\n", ret);
		return ret;
	}
	fp = fopen(file_name, "w");
	printf("Generate bin file size:%ld\n", config_size);
	n = fwrite(cfg_buf, 1, config_size, fp);
	if (n != config_size) {
		printf("Write to %s failed. Write %d bytes\n", file_name, n);
	}
	fclose(fp);

	hbn_free_gdc_bin(cfg_buf);

	return ret;
}