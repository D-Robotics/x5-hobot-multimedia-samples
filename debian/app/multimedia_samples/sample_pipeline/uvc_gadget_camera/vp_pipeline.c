#include"vp_pipeline.h"

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
	uint32_t hw_id = 0;
	int32_t ret = 0;
	uint32_t chn_id = 0;
	uint64_t vin_attr_ex_mask = 0;

	sensor_config = pipe_contex->sensor_config;
	vin_node_attr = sensor_config->vin_node_attr;
	vin_ichn_attr = sensor_config->vin_ichn_attr;
	vin_ochn_attr = sensor_config->vin_ochn_attr;
	// 调整 mipi_rx 的 index
	// vin_node_attr->cim_attr.mipi_rx = active_mipi_host;
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
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;

	printf("hbn_vnode_set_ochn_attr: %dx%d fps:%d\n", vse_ochn_attr[vse_bind_index].target_w,
		vse_ochn_attr[vse_bind_index].target_h, output_info->fps);
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
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->vse_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
							pipe_contex->vin_node_handle,
							1,
							pipe_contex->isp_node_handle,
							0);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
							pipe_contex->isp_node_handle,
							0,
							pipe_contex->vse_node_handle,
							0);
	ERR_CON_EQ(ret, 0);

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