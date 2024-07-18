#ifndef __VP_SENSORS_H__
#define __VP_SENSORS_H__

#include <string.h>

#include "vin_cfg.h"
#include "isp_cfg.h"
#include "hb_camera_data_config.h"
#include "cam_def.h"

// Todo: remove define variable
#define MAGIC_NUMBER 0x12345678
#define AUTO_ALLOC_ID -1

#define VP_MAX_BUF_SIZE 256
#define VP_MAX_VCON_NUM 4

typedef struct {
	int index;
	int is_valid;
	int mclk_is_not_configed;
	char sensor_config_list[128];
} csi_info_t;
//保证 0-3 的信息分别存储到 csi_info中，即使这个CSI下没有摄像头
typedef struct{
	int valid_count;
	int max_count;
	csi_info_t csi_info[VP_MAX_VCON_NUM];
} csi_list_info_t;

typedef struct vcon_properties {
	char device_path[VP_MAX_BUF_SIZE];
	char compatible[VP_MAX_BUF_SIZE];
	int32_t type;
	int32_t bus;
	int32_t rx_phy[2];
	char status[VP_MAX_BUF_SIZE];
	char pinctrl_names[VP_MAX_BUF_SIZE];
	int32_t pinctrl_0[8];
	int32_t gpio_oth[8];
} vcon_propertie_t;

typedef struct mipi_properties {
	char device_path[VP_MAX_BUF_SIZE];
	char status[VP_MAX_BUF_SIZE];
	char pinctrl_names[VP_MAX_BUF_SIZE];
	int32_t pinctrl_0[8];
	int32_t pinctrl_1[8];
	int32_t snrclk_idx[8];
} mipi_propertie_t;

typedef struct vp_csi_config_s{
	int index;
	int mclk_is_not_configed;
}vp_csi_config_t;

typedef struct vp_sensor_config_s {
	int16_t chip_id_reg;
	int16_t chip_id;
	// Some sensors use a different set of i2c addresses
	uint32_t sensor_i2c_addr_list[8];
	char sensor_name[128];
	char config_file[128];
	camera_config_t *camera_config;
	vin_node_attr_t *vin_node_attr;
	vin_ichn_attr_t *vin_ichn_attr;
	vin_ochn_attr_t *vin_ochn_attr;
	isp_attr_t      *isp_attr;
	isp_ichn_attr_t *isp_ichn_attr;
	isp_ochn_attr_t *isp_ochn_attr;
} vp_sensor_config_t;

extern vp_sensor_config_t *vp_sensor_config_list[];

uint32_t vp_get_sensors_list_number();
void vp_show_sensors_list();
vp_sensor_config_t *vp_get_sensor_config_by_name(char *sensor_name);
void vp_sensor_detect_structed(csi_list_info_t *csi_list_info);

int32_t vp_sensor_fixed_mipi_host(vp_sensor_config_t *sensor_config, vp_csi_config_t* mipi_config);
int32_t vp_sensor_multi_fixed_mipi_host(vp_sensor_config_t *sensor_config, int used_mipi_host, vp_csi_config_t* mipi_config);
#endif // __VP_SENSORS_H__