#include "vp_sensors.h"

#define SENSOR_WIDTH  640
#define SENSOR_HEIGHT  360
#define SENSOE_FPS 200
#define RAW10 0x2B

static mipi_config_t ov9782_mipi_config = {
	.rx_enable = 1,
	.rx_attr = {
		.phy = 0,
		.lane = 2,
		.datatype = RAW10,
		.fps = SENSOE_FPS,
		.mclk = 1,
		.mipiclk = 1728,
		.width = SENSOR_WIDTH,
		.height = SENSOR_HEIGHT,
		.linelenth = 728,
		.framelenth = 545,
		.settle = 0,
		.channel_num = 1,
		.channel_sel = {0},
		.hsdTime = 0,
		.hsaTime = 0,
		.hbpTime = 0,
	},
	.rx_ex_mask = 0x40,
	.rx_attr_ex = {
		.stop_check_instart = 1,
	}
};

static camera_config_t ov9782_camera_config = {
	.name = "ov9782",
	.addr = 0x60,
	.sensor_mode = NORMAL_M,
	.fps = SENSOE_FPS,
	.format = RAW10,
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.gpio_enable_bit = 0x01,
	.gpio_level_bit = 0x00,
	.mipi_cfg = &ov9782_mipi_config,
	.calib_lname = "/usr/hobot/lib/sensor/ov9782_640x360_tuning.json",
};

static vin_node_attr_t ov9782_vin_node_attr = {
	.cim_attr = {
		.mipi_rx = 0,
		.vc_index = 0,
		.ipi_channel = 1,
		.cim_isp_flyby = 1,
		.func = {
			.enable_frame_id = 1,
			.set_init_frame_id = 1,
			.hdr_mode = NOT_HDR,
			.time_stamp_en = 1,
			.time_stamp_mode = 3,
			.ts_src = 1,
			.pps_src = 6,
		},

	},
};

static vin_attr_ex_t ov9782_vin_attr_ex = {
	.vin_attr_ex_mask = 0x80,
	.mclk_ex_attr = {
		.mclk_freq = 24000000,
	},
};

static vin_ichn_attr_t ov9782_vin_ichn_attr = {
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.format = RAW10,
};

static vin_ochn_attr_t ov9782_vin_ochn_attr = {
	.ddr_en = 1,
	.ochn_attr_type = VIN_BASIC_ATTR,
	.vin_basic_attr = {
		.format = RAW10,
		// 硬件 stride 跟格式匹配，通过行像素根据raw数据bit位数计算得来
		// 8bit：x1, 10bit: x2 12bit: x2 16bit: x2,例raw10，1920 x 2 = 3840
		.wstride = (SENSOR_WIDTH) * 2,
	},
};

static isp_attr_t ov9782_isp_attr = {
	.input_mode = 1, // 0: online, 1: mcm, 类似offline
	.sensor_mode= ISP_NORMAL_M,
	.crop = {
		.x = 0,
		.y = 0,
		.h = SENSOR_HEIGHT,
		.w = SENSOR_WIDTH,
	},
};

static isp_ichn_attr_t ov9782_isp_ichn_attr = {
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.fmt = FRM_FMT_RAW,
	.bit_width = 10,
};

static isp_ochn_attr_t ov9782_isp_ochn_attr = {
	.ddr_en = 1,
	.fmt = FRM_FMT_NV12,
	.bit_width = 8,
};

vp_sensor_config_t ov9782_linear_640x360_raw10_200fps_2lane = {
	.chip_id_reg = 0x300A,
	.chip_id = 0x9281,
	.sensor_i2c_addr_list = {0x60},
	.sensor_name = "ov9782-200fps-2lane",
	.config_file = "linear_640x360_raw10_200fps_2lane.c",
	.camera_config = &ov9782_camera_config,
	.vin_ichn_attr = &ov9782_vin_ichn_attr,
	.vin_node_attr = &ov9782_vin_node_attr,
	.vin_attr_ex   = &ov9782_vin_attr_ex,
	.vin_ochn_attr = &ov9782_vin_ochn_attr,
	.isp_attr      = &ov9782_isp_attr,
	.isp_ichn_attr = &ov9782_isp_ichn_attr,
	.isp_ochn_attr = &ov9782_isp_ochn_attr,
};
