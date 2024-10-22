#include "vp_sensors.h"

#define SENSOR_WIDTH  1280
#define SENSOR_HEIGHT  960
#define SENSOE_FPS 30
#define RAW12 0x2B

static mipi_config_t imx477_mipi_config = {
	.rx_enable = 1,
	.rx_attr = {
		.phy = 0,
		.lane = 2,
		.datatype = RAW12,
		.fps = SENSOE_FPS,
		.mclk = 24,
		.mipiclk = 2250,
		.width = SENSOR_WIDTH,
		.height = SENSOR_HEIGHT,
		.linelenth = 2976,
		.framelenth = 1313,
		.settle = 30,
		.channel_num = 1,
		.channel_sel = {0},
	},
	.rx_ex_mask = 0x40,
	.rx_attr_ex = {
		.stop_check_instart = 1,
	}
};

static camera_config_t imx477_camera_config = {
	.name = "imx477",
	.addr = 0x1a,
	.sensor_mode = 1,
	.fps = SENSOE_FPS,
	.format = RAW12,
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.gpio_enable_bit = 0x01,
	.gpio_level_bit = 0x00,
	.mipi_cfg = &imx477_mipi_config,
	.calib_lname = "/usr/hobot/bin/imx477_tuning_1280x960.json",
};

static vin_node_attr_t imx477_vin_node_attr = {
	.cim_attr = {
		.mipi_rx = 2,
		.vc_index = 0,
		.ipi_channel = 1,
		.cim_isp_flyby = 1,
		.func = {
			.enable_frame_id = 1,
			.set_init_frame_id = 0,
			.hdr_mode = NOT_HDR,
			.time_stamp_en = 0,
		},

	},
};

static vin_ichn_attr_t imx477_vin_ichn_attr = {
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.format = RAW12,
};

static vin_ochn_attr_t imx477_vin_ochn_attr = {
	.ddr_en = 1,
	.ochn_attr_type = VIN_BASIC_ATTR,
	.vin_basic_attr = {
		.format = RAW12,
		// 硬件 stride 跟格式匹配，通过行像素根据raw数据bit位数计算得来
		// 8bit：x1, 10bit: x2 12bit: x2 16bit: x2,例raw10，1920 x 2 = 3840
		.wstride = (SENSOR_WIDTH) * 2,
	},
};

static isp_attr_t imx477_isp_attr = {
	.input_mode = 0, // 0: online, 1: mcm, 类似offline
	.sensor_mode= ISP_NORMAL_M,
	.crop = {
		.x = 0,
		.y = 0,
		.h = SENSOR_HEIGHT,
		.w = SENSOR_WIDTH,
	},
};

static isp_ichn_attr_t imx477_isp_ichn_attr = {
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.fmt = FRM_FMT_RAW,
	.bit_width = 12,
};

static isp_ochn_attr_t imx477_isp_ochn_attr = {
	.ddr_en = 1,
	.fmt = FRM_FMT_NV12,
	.bit_width = 8,
};

vp_sensor_config_t imx477_linear_1280x960_raw10_120fps_2lane = {
	.chip_id_reg = 0x0016,
	.chip_id = 0x0477,
	.sensor_i2c_addr_list = {0x1A},
	.sensor_name = "imx477-1280x960-120fps",
	.config_file = "linear_1280x960_raw10_120fps_2lane.c",
	.camera_config = &imx477_camera_config,
	.vin_ichn_attr = &imx477_vin_ichn_attr,
	.vin_node_attr = &imx477_vin_node_attr,
	.vin_ochn_attr = &imx477_vin_ochn_attr,
	.isp_attr      = &imx477_isp_attr,
	.isp_ichn_attr = &imx477_isp_ichn_attr,
	.isp_ochn_attr = &imx477_isp_ochn_attr,
};
