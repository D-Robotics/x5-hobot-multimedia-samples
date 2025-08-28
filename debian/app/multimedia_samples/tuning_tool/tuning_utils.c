/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <time.h>
#include "tuning_utils.h"


static char *module_name[] = DEF_MODULE_NAME;

static char *get_module_name(df_nmi_t mni)
{
	if (mni >= SIF_MNI && mni <= N2D_MNI)
		return module_name[mni];
	else
		return module_name[COM_MNI];
}

static inline int32_t is_buf_format_raw(hbn_vnode_image_t *out_img)
{
	return out_img->buffer.size[1] == 0;
}

void tuning_get_filename(char *name, char *path, hbn_vnode_image_t *out_img, df_nmi_t mni)
{
	struct tm *t;
	time_t tt;
	char *suffix = "yuv";
	char *df_name = get_module_name(mni);
	static uint32_t file_cnt = 0;

	time(&tt);
	t = localtime(&tt);

	if (is_buf_format_raw(out_img))
		suffix = "raw";

	snprintf(name, TUNING_PRINT_SIZE_MAX, "%s/%s_%d_b%d_f%d_%02d%02d%02d.%s", path, df_name, file_cnt++,
		out_img->info.bufferindex, out_img->info.frame_id, t->tm_hour, t->tm_min, t->tm_sec, suffix);
}

int32_t tuning_send_raw_to_hbplayer(tool_event_t *event, const hbn_vnode_image_t *normal_buf,
				enum RAW_BIT format, int32_t pipe_id)
{
	void *raw_addr = NULL;
	pic_info_t hbplayer_info;
	uint32_t size;

	if (normal_buf == NULL) {
		pr_tuning("NULL param set, err.\n");
		return -1;
	}

	hbplayer_info.format = format;
	hbplayer_info.type = RAW_DATA;
	hbplayer_info.frame_id = normal_buf->info.frame_id;
	hbplayer_info.width = normal_buf->buffer.width;
	hbplayer_info.height = normal_buf->buffer.height;
	hbplayer_info.stride = normal_buf->buffer.stride;
	hbplayer_info.chn_id = 0;
	hbplayer_info.pipe_id = pipe_id;

	raw_addr = normal_buf->buffer.virt_addr[0];
	size = normal_buf->buffer.size[0];

	if (size == 0 || raw_addr == NULL) {
		pr_tuning("invaild frame %d size w x h = %d x %d stride %d, skip send hbplayer\n",
				hbplayer_info.frame_id,	hbplayer_info.width,
				hbplayer_info.height, hbplayer_info.stride);
		return -1;
	}

	return hb_tool_send_raw_pic(event, &hbplayer_info, raw_addr, size, 0, 0);
}

int32_t tuning_send_yuv_to_hbplayer(tool_event_t *event, const hbn_vnode_image_t *normal_buf, int32_t pipe_id)
{
	void *plane0_addr = NULL;
	void *plane1_addr = NULL;
	pic_info_t hbplayer_info = {0};
	uint32_t size;

	if(normal_buf == NULL) {
		pr_tuning("NULL param set, err.\n");
		return -1;
	}

	hbplayer_info.pipe_id = pipe_id;
	hbplayer_info.format = YUVNV12;
	hbplayer_info.frame_id = normal_buf->info.frame_id;
	hbplayer_info.width = normal_buf->buffer.width;
	hbplayer_info.height = normal_buf->buffer.height;
	hbplayer_info.stride = normal_buf->buffer.stride;

	plane0_addr = normal_buf->buffer.virt_addr[0];
	plane1_addr = normal_buf->buffer.virt_addr[1];
	size = hbplayer_info.height * hbplayer_info.stride;

	if (size == 0 || plane0_addr == NULL || plane1_addr == NULL) {
		pr_tuning("invaild frame %d size w x h = %d x %d stride %d, skip send\n",
				hbplayer_info.frame_id,	hbplayer_info.width,
				hbplayer_info.height, hbplayer_info.stride);
		return -1;
	}

	return hb_tool_send_yuv_pic(event, &hbplayer_info, plane0_addr, size, plane1_addr, size / 2, 0, 0);
}

int32_t tuning_dump_file(char *filename, hbn_vnode_image_t *out_img)
{
	FILE *Fd = NULL;
	char *buffer = NULL;
	int32_t size;

	Fd = fopen(filename, "a");

	if (Fd == NULL) {
		pr_tuning("open %s fail", filename);
		return -1;
	}

	fflush(stdout);
	if (is_buf_format_raw(out_img)) {
		size = out_img->buffer.size[0];
		buffer = (char *)malloc(size);
		memcpy(buffer, (char *)out_img->buffer.virt_addr[0], size);
	} else {
		size = out_img->buffer.size[0] + out_img->buffer.size[1];
		buffer = (char *)malloc(size);
		memcpy(buffer, (char *)out_img->buffer.virt_addr[0], out_img->buffer.size[0]);
		memcpy(buffer + out_img->buffer.size[0], (char *)out_img->buffer.virt_addr[1], out_img->buffer.size[1]);
	}
	fwrite(buffer, 1, size, Fd);
	fflush(Fd);

	if (Fd)
		fclose(Fd);
	if (buffer)
		free(buffer);

	pr_tuning("filedump %s done\n", filename);

	return 0;
}

int32_t tuning_alloc_feedback_buffer(hb_mem_graphic_buf_t *buf, uint32_t width, uint32_t height, uint32_t cached)
{
	int32_t ret;
	int64_t alloc_flags = 0;

	ret = hb_mem_module_open();
	if (ret < 0) {
		pr_tuning("hb_mem_module_open failed ret %d\n", ret);
		return ret;
	}

	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED | HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD | HB_MEM_USAGE_CPU_READ_OFTEN |
		      HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	if (cached == 1)
		alloc_flags = alloc_flags | HB_MEM_USAGE_CACHED;

	ret = hb_mem_alloc_graph_buf(width, height, MEM_PIX_FMT_RAW10, alloc_flags, width*2, height, buf);
	if (ret < 0) {
		pr_tuning("hb_mem_alloc_graph_buf ret %d failed \n", ret);
		return ret;
	}

	return ret;
}

int32_t tuning_free_feedback_buffer(hb_mem_graphic_buf_t *buf)
{
	int32_t ret;

	ret = hb_mem_free_buf_with_vaddr((uint64_t)buf->virt_addr[0]);
	if (ret < 0) {
		pr_tuning("hb_mem_free_buf failed ret %d\n", ret);
		return ret;
	}

	ret = hb_mem_module_close();
	if (ret < 0) {
		pr_tuning("hb_mem_module_close failed ret %d\n", ret);
		return ret;
	}

	return ret;
}

int32_t tuning_get_raw_list(char *path, char img_path[][128], char img_name[][128], int32_t *img_num)
{
	struct dirent **namelist;
	int32_t file_cnt, file;
	int32_t img_count = 0;

	file_cnt = scandir(path, &namelist, NULL, alphasort);
	if (file_cnt == -1) {
		pr_tuning("scandir fail(%d)\n", file_cnt);
		return file_cnt;
	}

	for (file = 0; file < file_cnt; file++) {
		if (img_count >= TUNING_FEEDBACK_FILE_MAX) {
			pr_tuning("Warning: support feedback max raw img number: %d\n", TUNING_FEEDBACK_FILE_MAX);
			break;
		}

		if (!strstr(namelist[file]->d_name, ".raw")) {
			free(namelist[file]);
			continue;
		}

		strcpy((char *)&img_path[img_count], path);
		img_path[img_count][strlen(img_path[img_count])] = '/';
		strcat(img_path[img_count], namelist[file]->d_name);
		memcpy(img_name[img_count++], namelist[file]->d_name, strlen(namelist[file]->d_name)-sizeof("raw"));

		free(namelist[file]);
	}

	for (;file < file_cnt; file++)
		free(namelist[file]);

	*img_num = img_count;
	free(namelist);

	return 0;
}

struct timeval start, end;
void tuning_time_point()
{
#ifdef TUNING_API_DELAY_DEBUG
	gettimeofday(&start, NULL);
#endif
}

void tuning_time_delay(const char *func_name)
{
#ifdef TUNING_API_DELAY_DEBUG
	gettimeofday(&end, NULL);
	printf("Call %s delay %lds, %ldus\n", func_name, end.tv_sec - start.tv_sec, end.tv_usec - start.tv_usec);
#endif
}
