#include "vp_sensors.h"

#define SENSOR_WIDTH  2592
#define SENSOR_HEIGHT  1944
#define SENSOR_FPS 10
#define RAW10 0x2B
#define MICROSECONDS_PER_SECOND 1000000

static mipi_config_t mipi_config = {
	.rx_enable = 1,
	.rx_attr = {
		.phy = 0,
		.lane = 2, // 2,  // 4,
		.datatype = RAW10,
		.fps = SENSOR_FPS,
		.mclk = 1,
		.mipiclk = 2208,   // 2 lanes, 4416 for 4 lanes
		.width = SENSOR_WIDTH,
		.height = SENSOR_HEIGHT,
		.linelenth = 752,
		.framelenth = 2128,
		.settle = 0,
		.channel_num = 1,
		.channel_sel = {0},
	},
};

static camera_config_t camera_config = {
	.name = "ox05b1s",
	.addr = 0x36,
	.sensor_mode = SLAVE_M,// SLAVE_M,  // NORMAL_M,
	.fps = SENSOR_FPS,
	.format = RAW10,
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.mipi_cfg = &mipi_config,
	.gpio_enable_bit = 0x01,
	.gpio_level_bit = 0x00,
	.calib_lname = "disable",
};

static vin_node_attr_t vin_node_attr = {
	.cim_attr = {
		.mipi_rx = 0,  // vcon 0
		.vc_index = 0,
		.ipi_channel = 1,
		.cim_isp_flyby = 0,  // 0: offline ; 1: online, mcm
		.func = {
			.enable_frame_id = 1,
			.set_init_frame_id = 0,
			.hdr_mode = NOT_HDR,
			.time_stamp_en = 0,
		},
	},
	.lpwm_attr = {
		.enable = 1,
		.lpwm_chn_attr = {
			{	.trigger_source = 0,
				.trigger_mode = 0,
				.period = MICROSECONDS_PER_SECOND / SENSOR_FPS,  // 30fps: 333333;  10fps: 100000
				.offset = 10,
				.duty_time = 100,
				.threshold = 0,
				.adjust_step = 0,
			},
			{	.trigger_source = 0,
				.trigger_mode = 0,
				.period = MICROSECONDS_PER_SECOND / SENSOR_FPS,
				.offset = 10,
				.duty_time = 100,
				.threshold = 0,
				.adjust_step = 0,
			},
			{	.trigger_source = 0,
				.trigger_mode = 0,
				.period = MICROSECONDS_PER_SECOND / SENSOR_FPS,
				.offset = 10,
				.duty_time = 100,
				.threshold = 0,
				.adjust_step = 0,
			},
			{	.trigger_source = 0,
				.trigger_mode = 0,
				.period = MICROSECONDS_PER_SECOND / SENSOR_FPS,
				.offset = 10,
				.duty_time = 100,
				.threshold = 0,
				.adjust_step = 0,
			},
		},
	},
};

static vin_attr_ex_t vin_attr_ex = {
	.vin_attr_ex_mask = 0x80,
	.mclk_ex_attr = {
		.mclk_freq = 24000000,
	},
};

static vin_ichn_attr_t vin_ichn_attr = {
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.format = RAW10,
};

static vin_ochn_attr_t vin_ochn_attr = {
	.ddr_en = 1,
	.ochn_attr_type = VIN_BASIC_ATTR,
	.vin_basic_attr = {
		.format = RAW10,
		// 硬件 stride 跟格式匹配，通过行像素根据 raw 数据 bit 位数计算得来
		// 8bit： x1, 10bit: x2 12bit: x2 16bit: x2, 例 raw10 ， 1920 x 2 = 3840
		.wstride = (SENSOR_WIDTH) * 2,
		.pack_mode = 1,
	},
};

static isp_attr_t isp_attr = {
	.input_mode = 2, // 0: online, 1: mcm, 类似 offline; 2: offline
	.sensor_mode= ISP_NORMAL_M,
	.crop = {
		.x = 0,
		.y = 0,
		.h = SENSOR_HEIGHT,
		.w = SENSOR_WIDTH,
	},
};

static isp_ichn_attr_t isp_ichn_attr = {
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.fmt = FRM_FMT_RAW,
	.bit_width = 10,
};

static isp_ochn_attr_t isp_ochn_attr = {
	.ddr_en = 1,
	.fmt = FRM_FMT_NV12,
	.bit_width = 8,
};

vp_sensor_config_t ox05b1s_linear_2592x1944_raw10_10fps_2lane = {
	.chip_id_reg = 0x300a,
	.chip_id = 0x0058,
	.sensor_i2c_addr_list = {0x36},
	.sensor_type = SENSOR_TYPE_NORMAL,
	.sensor_name = "ox05b1s_2lane",
	.config_file = "linear_2592x1944_raw10_10fps_2lane.c",
	.camera_config = &camera_config,
	.vin_ichn_attr = &vin_ichn_attr,
	.vin_node_attr = &vin_node_attr,
	.vin_attr_ex   = &vin_attr_ex,
	.vin_ochn_attr = &vin_ochn_attr,
	.isp_attr      = &isp_attr,
	.isp_ichn_attr = &isp_ichn_attr,
	.isp_ochn_attr = &isp_ochn_attr,
};
