#include "vp_sensors.h"

// 曝光17次，合并成一个长帧从mipi输出，每帧加一行数据做间隔
// 224 x 1903 的分辨率，推算出每帧分辨率：224 x [(173 - 11) / 11 = 172]
#define SENSOR_WIDTH  224
#define SENSOR_HEIGHT  1903
#define SENSOE_FPS 5
#define RAW12 0x2C

static mipi_config_t mipi_config = {
	.rx_enable = 1,
	.rx_attr = {
		.phy = 0, // 代表 mipi phy，目前只支持设置为 0
		.lane = 2,
		.datatype = RAW12,
		.fps = SENSOE_FPS,
		.mclk = 1,
		// 时钟： 500Mhz/lane 硬件上的mipi传输时钟（示波器测量到的信号频率）
		// 数据率 = 时钟 * 2 双边延采样，所以数据率需要乘以 2 为 1000Mbps/lane
		// 参数 mipiclk = 数据率 * （使用的data lane数量）， 我们使用 lane = 2 ，所以配置为 2000
		.mipiclk = 320 * 2,
		.width = SENSOR_WIDTH,
		.height = SENSOR_HEIGHT,
		.linelenth = 353,
		.framelenth = 28000,
		.settle = 60,
		.channel_num = 1,
		.channel_sel = {0},
	},
	// .rx_ex_mask = 0x1, // 使能 hs nocheck 扩展参数的掩码，只有设置了掩码，对应的参数才能生效
	// .rx_attr_ex = {
	// 	.nocheck = 1, // ToF 做slave模式，初始化的时候不需要检查是否进入 hs reception, 做master时则必须要做这个检查
	// }
};

static camera_config_t camera_config = {
	.name = "irs2381c",
	.addr = 0x3d,
	.sensor_mode = NORMAL_M,
	.fps = SENSOE_FPS,
	.format = RAW12,
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.gpio_enable_bit = 0x07,
	.gpio_level_bit = 0x00,
	.mipi_cfg = &mipi_config,
	.calib_lname = "disable",
};

static vin_node_attr_t vin_node_attr = {
	.cim_attr = {
		.mipi_rx = 1, // 使用的mipi host 序号，支持 0, 1, 2, 3
		.vc_index = 0,
		.ipi_channel = 1,
		// SIF online/offline ISP
		.cim_isp_flyby = 1,
		.func = {
			// 帧id功能使能后会在raw图像开始位置添加帧id值
			// ToF 数据的帧开始两个2字节不能被改写，所以这里不能使能 frame_id
			.enable_frame_id = 0,
			.set_init_frame_id = 0,
			.hdr_mode = NOT_HDR,
			.time_stamp_en = 0,
		},

	},
};

static vin_ichn_attr_t vin_ichn_attr = {
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.format = RAW12,
};

static vin_ochn_attr_t vin_ochn_attr = {
	// 使能数据输出至DDR
	.ddr_en = 1,
	.ochn_attr_type = VIN_BASIC_ATTR,
	.vin_basic_attr = {
		.format = RAW12,
		// 硬件 stride 跟格式匹配，通过行像素根据raw数据bit位数计算得来
		// 8bit：x1, 10bit: x2 12bit: x2 16bit: x2,例raw10，1920 x 2 = 3840
		.wstride = (SENSOR_WIDTH) * 2,
	},
};

static isp_attr_t isp_attr = {
	.input_mode = 0,
	// 使用Linear模式
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
	.bit_width = 12,
};

static isp_ochn_attr_t isp_ochn_attr = {
	.ddr_en = 1,
	.fmt = FRM_FMT_NV12,
	.bit_width = 8,
};

vp_sensor_config_t irs2381c_linear_224x1903_raw12_5fps_2lane = {
	.chip_id_reg = 0xA0A4,
	.chip_id = 0x2381,
	.sensor_name = "irs2381c-tof",
	.config_file = "linear_224x1903_raw12_5fps_2lane.c",
	.camera_config = &camera_config,
	.vin_ichn_attr = &vin_ichn_attr,
	.vin_node_attr = &vin_node_attr,
	.vin_ochn_attr = &vin_ochn_attr,
	.isp_attr      = &isp_attr,
	.isp_ichn_attr = &isp_ichn_attr,
	.isp_ochn_attr = &isp_ochn_attr,
};
