#include "vp_sensors.h"

#define SENSOR_WIDTH  1920
#define SENSOR_HEIGHT  1080
#define SENSOE_FPS 30

#define RAW12 0x2C

static mipi_config_t ar0233_mipi_config = {
	.rx_enable = 1,
	.rx_attr = {
		.phy = 0,
		.lane = 2,
		.datatype = RAW12,
		.fps = SENSOE_FPS,
		.mclk = 24,
		.mipiclk = 2400,
		.width = SENSOR_WIDTH,
		.height = SENSOR_HEIGHT,
		.linelenth = 4440,
		.framelenth = 3003,
		.settle = 0,
		.channel_num = 1,
		.channel_sel = {0},
	},
};

static deserial_config_t ar0233_deserial_config = {
	.name = "max96712",
	.addr = 0x29,
	.mipi_cfg = &ar0233_mipi_config,
};

static camera_config_t ar0233_camera_config = {
	.name = "ar0233",
	.addr = 0x11,
	.serial_addr = 0x41,
	.eeprom_addr = 0x51,
	.sensor_mode = PWL_M,
	.fps = SENSOE_FPS,
	.format = RAW12,
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.extra_mode = 7,
	.config_index = 0,
	.gpio_enable_bit = 0x07,
	.gpio_level_bit = 0,
	.mipi_cfg = &ar0233_mipi_config,
	.calib_lname = "disable",

};

static vin_node_attr_t ar0233_vin_node_attr = {
	.cim_attr = {
		.mipi_rx = 0,
		.vc_index = 0,
		.ipi_channel = 1,
		.cim_isp_flyby = 1,
		.func = {
			.enable_frame_id = 1,
			.set_init_frame_id = 1,
			.hdr_mode = NOT_HDR,
			.time_stamp_en = 0,
			.enable_pattern = 0,
		},

	},
};

static vin_attr_ex_t ar0233_vin_attr_ex = {
	.vin_attr_ex_mask = 0x80,
	.mclk_ex_attr = {
		.mclk_freq = 24000000,
	},
};

static vin_ichn_attr_t ar0233_vin_ichn_attr = {
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.format = RAW12,
};

static vin_ochn_attr_t ar0233_vin_ochn_attr = {
	.ddr_en = 1,
	.ochn_attr_type = VIN_BASIC_ATTR,
	.vin_basic_attr = {
		.format = RAW12,
		// 硬件 stride 跟格式匹配，通过行像素根据raw数据bit位数计算得来
		// 8bit：x1, 10bit: x2 12bit: x2 16bit: x2,例raw10，1920 x 2 = 3840
		.wstride = (SENSOR_WIDTH) * 2,
	},
};


vp_sensor_config_t ar0233_linear_1920x1080_raw12_30fps_2lane = {
	.chip_id_reg = 0x3107,
	.chip_id = 0xcb34,
	.sensor_i2c_addr_list = {0x11},
	.sensor_name = "ar0233-30fps",
	.config_file = "linear_1920x1080_raw12_30fps_2lane.c",
	.camera_config = &ar0233_camera_config,
	.vin_ichn_attr = &ar0233_vin_ichn_attr,
	.vin_node_attr = &ar0233_vin_node_attr,
	.vin_attr_ex   = &ar0233_vin_attr_ex,
	.vin_ochn_attr = &ar0233_vin_ochn_attr,
	.deserial_node_attr = &ar0233_deserial_config,
	.mipi_cfg_attr = &ar0233_mipi_config,
};
