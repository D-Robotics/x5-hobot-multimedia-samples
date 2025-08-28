#include "vp_sensors.h"

#define SENSOR_WIDTH  1920
#define SENSOR_HEIGHT  1080
#define SENSOE_FPS 30


static mipi_config_t ar0233_mipi_config = {
	.rx_enable = 1,
	.rx_attr = {
		.phy = 0,
		.lane = 2,
		.datatype = SENSOR_DATA_TYPE_RAW12,
		.fps = SENSOE_FPS,
		.mclk = 24,
		.mipiclk = 2400,
		.width = SENSOR_WIDTH,
		.height = SENSOR_HEIGHT,
		.linelenth = 4440,
		.framelenth = 3003,
		.settle = 0,
		.channel_num = 2,
		.channel_sel = {0,1},
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
	.format = SENSOR_DATA_TYPE_RAW12,
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
	.format = SENSOR_DATA_TYPE_RAW12,

};


static vin_ochn_attr_t ar0233_vin_ochn_attr = {

	.ddr_en = 1,
	.ochn_attr_type = VIN_BASIC_ATTR,
	.vin_basic_attr = {
		.format = SENSOR_DATA_TYPE_RAW12,
		.wstride = (SENSOR_WIDTH) * 2,
	},

};

static isp_attr_t ar0233_isp_attr = {
	.input_mode = 1, // 0: online, 1: mcm, 类似offline
	.sensor_mode= ISP_NORMAL_M,
	.crop = {
		.x = 0,
		.y = 0,
		.h = SENSOR_HEIGHT,
		.w = SENSOR_WIDTH,
	},
};

static isp_ichn_attr_t ar0233_isp_ichn_attr = {
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.fmt = FRM_FMT_RAW,
	.bit_width = 12,
};

static isp_ochn_attr_t ar0233_isp_ochn_attr = {
	.ddr_en = 1,
	.fmt = FRM_FMT_NV12,
	.bit_width = 8,
};

vp_sensor_config_t ar0233_linear_1920x1080_raw12_30fps_2lane_vc0 = {
	.chip_id_reg = 0x3107,
	.chip_id = 0xa55a,
	.sensor_i2c_addr_list = {0x11},
	.sensor_name = "ar0233-30fps",
	.sensor_type = SENSOR_TYPE_GMSL_RAW,
	.config_file = "ar0233_linear_1920x1080_raw12_30fps_2lane_vc0.c",
	.camera_config = &ar0233_camera_config,
	.vin_ichn_attr = &ar0233_vin_ichn_attr,
	.vin_node_attr = &ar0233_vin_node_attr,
	.vin_attr_ex = &ar0233_vin_attr_ex,
	.vin_ochn_attr = &ar0233_vin_ochn_attr,
	.isp_attr      = &ar0233_isp_attr,
	.isp_ichn_attr = &ar0233_isp_ichn_attr,
	.isp_ochn_attr = &ar0233_isp_ochn_attr,
	.deserial_node_attr = &ar0233_deserial_config,
	.mipi_cfg_attr = &ar0233_mipi_config,
};
