#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"

#include"vp_pipeline.h"
typedef struct {
    char *sensor_name;
    char *gdc_file_name;
    int is_valid;
} gdc_list_info_t;

static gdc_list_info_t g_gdc_list_info[] = {
    {
        .sensor_name = "sc202cs",
        .gdc_file_name = "./gdc_bin/sc202cs_gdc.bin",
        .is_valid = -1
    },
	{
        .sensor_name = "sc230ai",
        .gdc_file_name = "./gdc_bin/sc230ai_gdc.bin",
        .is_valid = -1
    }
};


const char *vp_gdc_get_bin_file(const char *sensor_name){
    char *gdc_file = NULL;

    for(int i = 0; i < sizeof(g_gdc_list_info)/sizeof(gdc_list_info_t); i++){
        int config_sensor_name_len = strlen(g_gdc_list_info[i].sensor_name);
        int ret = strncmp(sensor_name, g_gdc_list_info[i].sensor_name, config_sensor_name_len);
        if(ret == 0){
            gdc_file = g_gdc_list_info[i].gdc_file_name;
            break;
        }
    }

	if ((gdc_file != NULL) && access(gdc_file, F_OK) != 0) {
		printf("not found gdc file %s, so return null.\n", gdc_file);
		return NULL;
	}
    return gdc_file;
}

static int get_gdc_config(const char *gdc_bin_file, hb_mem_common_buf_t *bin_buf) {
	int64_t alloc_flags = 0;
	int ret = 0;
	int offset = 0;
	char *cfg_buf = NULL;

	FILE *fp = fopen(gdc_bin_file, "r");
	if (fp == NULL) {
		printf("File %s open failed\n", gdc_bin_file);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	long file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	cfg_buf = malloc(file_size);
	int n = fread(cfg_buf, 1, file_size, fp);
	if (n != file_size) {
        free(cfg_buf);
		printf("Read file size failed\n");
        fclose(fp);
        return -1;
	}
	fclose(fp);

	memset(bin_buf, 0, sizeof(hb_mem_common_buf_t));
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED | HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD | HB_MEM_USAGE_CPU_READ_OFTEN |
				HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	ret = hb_mem_alloc_com_buf(file_size, alloc_flags, bin_buf);
	if (ret != 0 || bin_buf->virt_addr == NULL) {
        free(cfg_buf);
		printf("hb_mem_alloc_com_buf for bin failed, ret = %d\n", ret);
		return -1;
	}

	memcpy(bin_buf->virt_addr, cfg_buf, file_size);
	ret = hb_mem_flush_buf(bin_buf->fd, offset, file_size);
	if (ret != 0 || bin_buf->virt_addr == NULL) {
        free(cfg_buf);
		printf("hb_mem_flush_buf for bin failed, ret = %d\n", ret);
		return -1;
	}

    free(cfg_buf);

	return ret;
}
#if 0
static int32_t destory_gdc_node(pipe_contex_t *pipe_contex){
	if(pipe_contex->gdc_node_handle != 0){
		hbn_vnode_close(pipe_contex->gdc_node_handle);
		hb_mem_free_buf(pipe_contex->bin_buf.fd);
	}
    return 0;
}
#endif

static int create_gdc_node(pipe_contex_t *pipe_contex, char *sensor_name) {
	int ret = 0;
	isp_ichn_attr_t isp_ichn_attr = {0};

	ret = hbn_vnode_get_ichn_attr(pipe_contex->isp_node_handle, 0, &isp_ichn_attr);
	ERR_CON_EQ(ret, 0);
	int input_width = isp_ichn_attr.width;
	int input_height = isp_ichn_attr.height;

    const char* gdc_bin_file = vp_gdc_get_bin_file(sensor_name);
	if(gdc_bin_file == NULL){
		printf("%s is enable gdc, but gdc bin file is not set.\n", sensor_name);
		return -1;
	}
    ret = get_gdc_config(gdc_bin_file, &pipe_contex->bin_buf);
	if(ret != 0){
		printf("%s is enable gdc, but gdc bin file [%s] is not valid.\n",
			sensor_name, gdc_bin_file);
		return -1;
	}
	printf("gdc input resolution: %d*%d, camera name [%s]\n", input_width, input_height, sensor_name);
	uint32_t hw_id = 0;
	ret = hbn_vnode_open(HB_GDC, hw_id, AUTO_ALLOC_ID, &(pipe_contex->gdc_node_handle));
	if(ret != 0){
		printf("%s is enable gdc and gdc bin file [%s] is valid, but open failed %d.\n",
			sensor_name, gdc_bin_file, ret);
		return -1;
	}
	gdc_attr_t gdc_attr = {0};
	gdc_attr.config_addr = pipe_contex->bin_buf.phys_addr;
	gdc_attr.config_size = pipe_contex->bin_buf.size;
	gdc_attr.binary_ion_id = pipe_contex->bin_buf.share_id;
	gdc_attr.binary_offset = pipe_contex->bin_buf.offset;
	gdc_attr.total_planes = 2;
	gdc_attr.div_width = 0;
	gdc_attr.div_height = 0;
	ret = hbn_vnode_set_attr(pipe_contex->gdc_node_handle, &gdc_attr);
	if(ret != 0){
		printf("%s is enable gdc and gdc bin file [%s] is valid, but set attr failed %d.\n",
			sensor_name, gdc_bin_file, ret);
		return -1;
	}

	uint32_t chn_id = 0;

	gdc_ichn_attr_t gdc_ichn_attr = {0};
	gdc_ichn_attr.input_width = input_width;
	gdc_ichn_attr.input_height = input_height;
	gdc_ichn_attr.input_stride = input_width;
	ret = hbn_vnode_set_ichn_attr(pipe_contex->gdc_node_handle, chn_id, &gdc_ichn_attr);
	if(ret != 0){
		printf("%s is enable gdc and gdc bin file [%s] is valid, but set ichn failed %d.\n",
			sensor_name, gdc_bin_file, ret);
		return -1;
	}

	gdc_ochn_attr_t gdc_ochn_attr = {0};
	gdc_ochn_attr.output_width = input_width;
	gdc_ochn_attr.output_height = input_height;
	gdc_ochn_attr.output_stride = input_width;
	ret = hbn_vnode_set_ochn_attr(pipe_contex->gdc_node_handle, chn_id, &gdc_ochn_attr);
	if(ret != 0){
		printf("%s is enable gdc and gdc bin file [%s] is valid, but set ochn failed %d.\n",
			sensor_name, gdc_bin_file, ret);
		return -1;
	}
	hbn_buf_alloc_attr_t alloc_attr = {0};
	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
		| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;

	ret = hbn_vnode_set_ochn_buf_attr(pipe_contex->gdc_node_handle, chn_id, &alloc_attr);
	if(ret != 0){
		printf("%s is enable gdc and gdc bin file [%s] is valid, but set ochn buffer failed %d.\n",
			sensor_name, gdc_bin_file, ret);
		return -1;
	}

    return 0;
}
static int create_camera_node(pipe_contex_t *pipe_contex, uint32_t sensor_mode)
{
	camera_config_t *camera_config = NULL;
	vp_sensor_config_t *sensor_config = NULL;
	int32_t ret = 0;

	sensor_config = pipe_contex->sensor_config;
	camera_config = sensor_config->camera_config;
	if (sensor_mode >= NORMAL_M && sensor_mode < INVALID_MOD) {
		camera_config->sensor_mode = sensor_mode;
		sensor_config->vin_node_attr->lpwm_attr.enable = 1;
	}
	ret = hbn_camera_create(camera_config, &pipe_contex->cam_fd);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static int create_vin_node(pipe_contex_t *pipe_contex, int active_mipi_host) {
	vp_sensor_config_t *sensor_config = NULL;
	vin_node_attr_t *vin_node_attr = NULL;
	vin_ichn_attr_t *vin_ichn_attr = NULL;
	vin_ochn_attr_t *vin_ochn_attr = NULL;
	hbn_vnode_handle_t *vin_node_handle = NULL;
	vin_attr_ex_t vin_attr_ex;
	hbn_buf_alloc_attr_t alloc_attr = {0};

	uint32_t hw_id = 0;
	int32_t ret = 0;
	uint32_t chn_id = 0;
	uint64_t vin_attr_ex_mask = 0;

	sensor_config = pipe_contex->sensor_config;
	vin_node_attr = sensor_config->vin_node_attr;
	vin_ichn_attr = sensor_config->vin_ichn_attr;
	vin_ochn_attr = sensor_config->vin_ochn_attr;
	// 调整 mipi_rx 的 index
	vin_node_attr->cim_attr.mipi_rx = active_mipi_host;
	hw_id = vin_node_attr->cim_attr.mipi_rx;
	vin_node_handle = &pipe_contex->vin_node_handle;

	if(pipe_contex->csi_config.mclk_is_not_configed){
		//设备树中没有配置mclk：使用外部晶振
		printf("csi%d ignore mclk ex attr, because not config mclk.\n",
			pipe_contex->csi_config.index);
		vin_attr_ex.vin_attr_ex_mask = 0x00;
	}else{
		vin_attr_ex.vin_attr_ex_mask = 0x80;	//bit7 for mclk
		vin_attr_ex.mclk_ex_attr.mclk_freq = 24000000; // 24MHz
	}

	ret = hbn_vnode_open(HB_VIN, hw_id, AUTO_ALLOC_ID, vin_node_handle);
	ERR_CON_EQ(ret, 0);
	// 设置基本属性
	ret = hbn_vnode_set_attr(*vin_node_handle, vin_node_attr);
	ERR_CON_EQ(ret, 0);
	// 设置输入通道的属性
	ret = hbn_vnode_set_ichn_attr(*vin_node_handle, chn_id, vin_ichn_attr);
	ERR_CON_EQ(ret, 0);
	// 设置输出通道的属性

	// 使能DDR输出
	vin_ochn_attr->ddr_en = 1;
	ret = hbn_vnode_set_ochn_attr(*vin_node_handle, chn_id, vin_ochn_attr);
	ERR_CON_EQ(ret, 0);
	vin_attr_ex_mask = vin_attr_ex.vin_attr_ex_mask;
	if (vin_attr_ex_mask) {
		for (uint8_t i = 0; i < VIN_ATTR_EX_INVALID; i ++) {
			if ((vin_attr_ex_mask & (1 << i)) == 0)
				continue;
			vin_attr_ex.ex_attr_type = i;
			/*we need to set hbn_vnode_set_attr_ex in a loop*/
			ret = hbn_vnode_set_attr_ex(*vin_node_handle, &vin_attr_ex);
			ERR_CON_EQ(ret, 0);
		}
	}
	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED
						| HB_MEM_USAGE_HW_CIM
						| HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	ret = hbn_vnode_set_ochn_buf_attr(*vin_node_handle, chn_id, &alloc_attr);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static int create_isp_node(pipe_contex_t *pipe_contex) {
	vp_sensor_config_t *sensor_config = NULL;
	isp_attr_t      *isp_attr = NULL;
	isp_ichn_attr_t *isp_ichn_attr = NULL;
	isp_ochn_attr_t *isp_ochn_attr = NULL;
	hbn_vnode_handle_t *isp_node_handle = NULL;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	uint32_t chn_id = 0;
	int ret = 0;

	sensor_config = pipe_contex->sensor_config;
	isp_attr = sensor_config->isp_attr;
	isp_ichn_attr = sensor_config->isp_ichn_attr;
	isp_ochn_attr = sensor_config->isp_ochn_attr;
	isp_node_handle = &pipe_contex->isp_node_handle;
	isp_attr->input_mode = 2; // offline

	ret = hbn_vnode_open(HB_ISP, 0, AUTO_ALLOC_ID, isp_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_attr(*isp_node_handle, isp_attr);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ochn_attr(*isp_node_handle, chn_id, isp_ochn_attr);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ichn_attr(*isp_node_handle, chn_id, isp_ichn_attr);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(*isp_node_handle, chn_id, &alloc_attr);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static int create_vse_node(pipe_contex_t *pipe_contex, int vse_bind_index, camera_config_info_t* output_info) {
	int ret = 0;
	hbn_vnode_handle_t *vse_node_handle = &pipe_contex->vse_node_handle;
	isp_ichn_attr_t isp_ichn_attr = {0};
	vse_attr_t vse_attr = {0};
	vse_ichn_attr_t vse_ichn_attr = {0};
	vse_ochn_attr_t vse_ochn_attr[VSE_MAX_CHANNELS] = {0};
	uint32_t chn_id = 0;
	uint32_t hw_id = 0;
	uint32_t input_width = 0, input_height = 0;
	uint32_t output_width = 0, output_height = 0;
	hbn_buf_alloc_attr_t alloc_attr = {0};

	ret = hbn_vnode_get_ichn_attr(pipe_contex->isp_node_handle, chn_id, &isp_ichn_attr);
	ERR_CON_EQ(ret, 0);
	input_width = isp_ichn_attr.width;
	input_height = isp_ichn_attr.height;

	vse_ichn_attr.width = input_width;
	vse_ichn_attr.height = input_height;
	vse_ichn_attr.fmt = FRM_FMT_NV12;
	vse_ichn_attr.bit_width = 8;

	vse_ochn_attr[vse_bind_index].chn_en = CAM_TRUE;
	vse_ochn_attr[vse_bind_index].roi.x = 0;
	vse_ochn_attr[vse_bind_index].roi.y = 0;
	vse_ochn_attr[vse_bind_index].roi.w = input_width;
	vse_ochn_attr[vse_bind_index].roi.h = input_height;
	vse_ochn_attr[vse_bind_index].fmt = FRM_FMT_NV12;
	vse_ochn_attr[vse_bind_index].bit_width = 8;

	configure_vse_max_resolution(vse_bind_index,
		input_width, input_height,
		&output_width, &output_height);

	// 输出原分辨率
	vse_ochn_attr[vse_bind_index].target_w = output_info->width;
	vse_ochn_attr[vse_bind_index].target_h = output_info->height;
	// vse_ochn_attr[vse_bind_index].fps = output_info->fps;
	ret = hbn_vnode_open(HB_VSE, hw_id, AUTO_ALLOC_ID, vse_node_handle);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_attr(*vse_node_handle, &vse_attr);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_ichn_attr(*vse_node_handle, chn_id, &vse_ichn_attr);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
		| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;

	printf("hbn_vnode_set_ochn_attr :vse channel %d, %dx%d fps:%d\n",
		vse_bind_index,
		vse_ochn_attr[vse_bind_index].target_w,
		vse_ochn_attr[vse_bind_index].target_h,
		output_info->fps);
	ret = hbn_vnode_set_ochn_attr(*vse_node_handle, vse_bind_index, &vse_ochn_attr[vse_bind_index]);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ochn_buf_attr(*vse_node_handle, vse_bind_index, &alloc_attr);
	ERR_CON_EQ(ret, 0);

	return 0;
}
int vp_get_vse_channel(int input_width, int input_height, int output_width, int output_height){
	int input_size = input_width * input_height;
	int output_size = output_width * output_height;
	//Upscale
	if(input_size < output_size){
#if 0
		if(output_size > input_size * 4){
			return -1;
		}
#endif
		return 5;
	}else{
		return 0;
	}
}
int vp_create_and_start_pipeline(pipe_contex_t *pipe_contex, vp_pipeline_info_t* vp_pipeline_info)
{
	int32_t ret = 0;

	// 创建pipeline中的每个node
	ret = create_camera_node(pipe_contex, vp_pipeline_info->sensor_mode);
	ERR_CON_EQ(ret, 0);
	ret = create_vin_node(pipe_contex, vp_pipeline_info->active_mipi_host);
	ERR_CON_EQ(ret, 0);
	ret = create_isp_node(pipe_contex);
	ERR_CON_EQ(ret, 0);

	if(vp_pipeline_info->enable_gdc){
		create_gdc_node(pipe_contex, vp_pipeline_info->sensor_name);
	}
	ret = create_vse_node(pipe_contex,
		vp_pipeline_info->vse_bind_index, &(vp_pipeline_info->camera_config_info));
	ERR_CON_EQ(ret, 0);

	// 创建HBN flow
	ret = hbn_vflow_create(&pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->isp_node_handle);
	ERR_CON_EQ(ret, 0);

	if(vp_pipeline_info->enable_gdc){
		ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
								pipe_contex->gdc_node_handle);
		ERR_CON_EQ(ret, 0);
	}

	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->vse_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
							pipe_contex->vin_node_handle,
							0,
							pipe_contex->isp_node_handle,
							0);
	ERR_CON_EQ(ret, 0);

	if(vp_pipeline_info->enable_gdc){
		ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
								pipe_contex->isp_node_handle,
								0,
								pipe_contex->gdc_node_handle,
								0);
		ERR_CON_EQ(ret, 0);

		ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
								pipe_contex->gdc_node_handle,
								0,
								pipe_contex->vse_node_handle,
								0);
		ERR_CON_EQ(ret, 0);
	}else{
		ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
								pipe_contex->isp_node_handle,
								0,
								pipe_contex->vse_node_handle,
								0);
		ERR_CON_EQ(ret, 0);
	}

	ret = hbn_camera_attach_to_vin(pipe_contex->cam_fd,
							pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	return 0;
}

int vp_destroy_and_stop_pipeline(pipe_contex_t *pipe_contex){
    int ret = 0;

    ret = hbn_vflow_stop(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	hbn_vflow_destroy(pipe_contex->vflow_fd);
    hbn_vnode_close(pipe_contex->vse_node_handle);
    hbn_vnode_close(pipe_contex->isp_node_handle);
    hbn_vnode_close(pipe_contex->vin_node_handle);
	hbn_camera_destroy(pipe_contex->cam_fd);

    return ret;
}
int vp_create_stop_vse_feedback_pieline(pipe_contex_t *pipe_contex){
    int ret = 0;

    ret = hbn_vflow_stop(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	hbn_vflow_destroy(pipe_contex->vflow_fd);

    hbn_vnode_close(pipe_contex->vse_node_handle);

    return ret;
}

int vp_create_start_vse_feedback_pieline(pipe_contex_t *pipe_contex, vp_vse_feedback_pipeline_info_t *pipeline_info){

	int ret = 0;
	hbn_vnode_handle_t *vse_node_handle = &pipe_contex->vse_node_handle;
	vse_attr_t vse_attr = {0};
	vse_ichn_attr_t vse_ichn_attr = {0};
	vse_ochn_attr_t vse_ochn_attr[VSE_MAX_CHANNELS] = {0};
	uint32_t chn_id = 0;
	uint32_t hw_id = 0;

	hbn_buf_alloc_attr_t alloc_attr = {0};
	int vse_bind_index = pipeline_info->vse_channel;

	vse_ichn_attr.width = pipeline_info->input_width;
	vse_ichn_attr.height = pipeline_info->input_height;
	vse_ichn_attr.fmt = FRM_FMT_NV12;
	vse_ichn_attr.bit_width = 8;

	vse_ochn_attr[vse_bind_index].chn_en = 1;
	vse_ochn_attr[vse_bind_index].roi.x = 0;
	vse_ochn_attr[vse_bind_index].roi.y = 0;
	vse_ochn_attr[vse_bind_index].roi.w = 0;
	vse_ochn_attr[vse_bind_index].roi.h = 0;
	vse_ochn_attr[vse_bind_index].fmt = FRM_FMT_NV12;
	vse_ochn_attr[vse_bind_index].bit_width = 8;

	// 输出原分辨率
	vse_ochn_attr[vse_bind_index].target_w = pipeline_info->output_width;
	vse_ochn_attr[vse_bind_index].target_h = pipeline_info->output_height;

	ret = hbn_vnode_open(HB_VSE, hw_id, AUTO_ALLOC_ID, vse_node_handle);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_attr(*vse_node_handle, &vse_attr);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_ichn_attr(*vse_node_handle, chn_id, &vse_ichn_attr);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
		| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;

	printf("hbn_vnode_set_ochn_attr :vse channel %d, %dx%d.\n",
		vse_bind_index,
		vse_ochn_attr[vse_bind_index].target_w,
		vse_ochn_attr[vse_bind_index].target_h);
	ret = hbn_vnode_set_ochn_attr(*vse_node_handle, vse_bind_index, &vse_ochn_attr[vse_bind_index]);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ochn_buf_attr(*vse_node_handle, vse_bind_index, &alloc_attr);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_create(&pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->vse_node_handle);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	return 0;
}
int vp_send_to_vse_feedback(pipe_contex_t *pipe_contex, int vse_channel, hbn_vnode_image_t* src){

	int ret = hbn_vnode_sendframe(pipe_contex->vse_node_handle, vse_channel, src);
	if (ret != 0) {
		printf("hbn_vnode_sendframe to vse failed(%d)\n", ret);
		return -1;
	}

	return 0;
}

int vp_get_from_vse_feedback(pipe_contex_t *pipe_contex, int vse_channel, hbn_vnode_image_t* src){

	int ret = hbn_vnode_getframe(pipe_contex->vse_node_handle, vse_channel, 2000 ,src);
	if (ret != 0) {
		printf("hbn_vnode_getframe to vse failed(%d)\n", ret);
		return -1;
	}
	return 0;
}

int vp_release_vse_feedback(pipe_contex_t *pipe_contex, int vse_channel, hbn_vnode_image_t* src){

	int ret = hbn_vnode_releaseframe(pipe_contex->vse_node_handle, vse_channel, src);
	if (ret != 0) {
		printf("hbn_vnode_releaseframe to vse failed(%d)\n", ret);
		return -1;
	}
	return 0;
}