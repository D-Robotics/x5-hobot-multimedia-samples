// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>

#include "utils/utils_log.h"
#include "utils/stream_define.h"
#include "utils/stream_manager.h"

#include "communicate/sdk_communicate.h"
#include "communicate/sdk_common_cmd.h"
#include "utils/common_utils.h"

#include "vpp_box_impl.h"
#include "vpp_camera_impl.h"
#include "bpu_wrap.h"

#include "solution_config.h"
#include "solution_handle.h"

// 这个文件中的接口后面都做成注册接口，实现对更底层sensor的封装
// 后面每个sensor或者应用示例实现一套接口就能对接上

typedef struct
{
	volatile enum
	{
		E_STATE_STARTED,
		E_STATE_RUNNING,
		E_STATE_STOPPING,
		E_STATE_STOPPED,
	} m_state;
	char		m_solution_name[32];
	vpp_ops_t	*impl;
} solution_handle_t;

static solution_handle_t *g_solution_handle = NULL;

static vpp_ops_t *box_generic_impl(void)
{
	vpp_ops_t *impl = malloc(sizeof(vpp_ops_t));

	memset(impl, 0, sizeof(vpp_ops_t));
	impl->init_param = vpp_box_init_param;
	impl->init = vpp_box_init;
	impl->uninit = vpp_box_uninit;
	impl->start = vpp_box_start;
	impl->stop = vpp_box_stop;
	impl->param_get = vpp_box_param_get;
	impl->param_set = vpp_box_param_set;
	impl->io_param_get = vpp_box_ion_param_get;
	return impl;
}

static vpp_ops_t *camera_generic_impl(void)
{
	vpp_ops_t *impl = malloc(sizeof(vpp_ops_t));

	memset(impl, 0, sizeof(vpp_ops_t));
	impl->init_param = vpp_camera_init_param;
	impl->init = vpp_camera_init;
	impl->uninit = vpp_camera_uninit;
	impl->start = vpp_camera_start;
	impl->stop = vpp_camera_stop;
	impl->param_get = vpp_camera_param_get;
	impl->param_set = vpp_camera_param_set;
	impl->io_param_get = vpp_camera_ion_param_get;

	return impl;
}

// 根据配置中的solution配置对应的接口函数
static int _solution_handle_init(solution_handle_t *handle)
{
	memset(handle, 0, sizeof(solution_handle_t));

	strcpy(handle->m_solution_name, g_solution_config.solution_name);

	if (strcmp(handle->m_solution_name, "cam_solution") == 0) {
		handle->impl = camera_generic_impl();
	} else if (strcmp(handle->m_solution_name, "box_solution") == 0) {
		handle->impl = box_generic_impl();
	} else {
		printf("Solution(%s) not implemented, Please look forward to it!\n", handle->m_solution_name);
			return -1;
	}

	SC_LOGI("Start Solution: %s", handle->m_solution_name);

	return 0;
}

/* id：用来区分不同的应用方案，比如选择使用哪个sensor，使用什么样的vps、venc配置，或者只启用vps和venc
 * 根据id来设置适用于该方案的接口方法
*/
int solution_handle_init(void)
{
	int ret = 0;
	solution_handle_t *handle;
	if (g_solution_handle)
		return 0;

	// 初始化配置
	solution_cfg_load();

	handle = malloc(sizeof(solution_handle_t));
	ASSERT(handle);
	g_solution_handle = handle;
	ret = _solution_handle_init(handle);
	if (ret)
	{
		SC_LOGE("_solution_handle_init failed!\n");
		goto err;
	}
	ASSERT(handle->impl);

	// 配置vin、 isp、vps、venc等各个模块的参数
	if (handle->impl->init_param && handle->impl->init_param())
	{
		SC_LOGE("handle->impl->init_param failed!\n");
		goto err;
	}

	// init vin、isp、vps、venc...
	if (handle->impl->init && handle->impl->init())
	{
		SC_LOGE("handle->impl->init failed!\n");
		goto err;
	}

	return 0;

err:
	if (handle)
		free(handle);
	g_solution_handle = NULL;

	return -1;
}

int solution_handle_uninit(void)
{
	solution_handle_t *handle = g_solution_handle;
	// 判断参数是否合法
	if (handle == NULL || handle->impl == NULL || handle->impl->uninit == NULL)
		return -1;
	if (handle->impl->uninit && handle->impl->uninit())
	{
		SC_LOGE("handle->impl->uninit failed!\n");
		return -1;
	}
	if (handle)
		free(handle);
	g_solution_handle = NULL;

	return 0;
}

int solution_handle_start(void)
{
	int ret = 0;
	solution_handle_t *handle = g_solution_handle;
	if (handle == NULL || handle->impl == NULL || handle->impl->start == NULL)
		return -1;
	ret = handle->impl->start();

	//for ion theory calc check
	sleep(1); //wait bpu init thread start
	solution_check_ion_theory_calc_result();
	return ret;
}
int solution_handle_stop(void)
{
	solution_handle_t *handle = g_solution_handle;
	if (handle == NULL || handle->impl == NULL || handle->impl->stop == NULL)
		return -1;
	return handle->impl->stop();
}

int solution_handle_get_config(char *out_str)
{
	char *config_str = solution_cfg_obj2string();
	strcpy(out_str, config_str);
	free(config_str);
	return 0;
}
int solution_handle_check_config(solution_check_info_t *check_info){
	solution_cfg_t solution_cfg;
	memset(&solution_cfg, 0, sizeof(solution_cfg_t));

	solution_ion_param_info_t solution_ion_param_info;
	solution_vpu_param_info_t solution_vpu_param_info;
	memset(&solution_ion_param_info, 0, sizeof(solution_ion_param_info));
	memset(&solution_vpu_param_info, 0, sizeof(solution_vpu_param_info));

	solution_cfg_string2obj_width_param(check_info->param, &solution_cfg);

	if (strcmp(solution_cfg.solution_name, "cam_solution") == 0) {
		vpp_camera_ion_param_get(&solution_cfg, &solution_ion_param_info);
	} else if (strcmp(solution_cfg.solution_name, "box_solution") == 0) {
		vpp_box_ion_param_get(&solution_cfg, &solution_ion_param_info);
		vpp_box_vpu_param_get(&solution_cfg, &solution_vpu_param_info);

	} else {
		SC_LOGE("Solution(%s) not implemented, Please look forward to it!", solution_cfg.solution_name);
			return -1;
	}
	check_info->ion_lack = solution_check_ion_is_enough(&solution_ion_param_info); //for ion
	check_info->vpu_lack = solution_check_vpu_is_enough(&solution_vpu_param_info); //for vpu
	return 0;
}

int solution_handle_set_config(char *in_str)
{
	solution_cfg_string2obj(in_str);
	return 0;
}

int solution_handle_save_config(char *in_str)
{
	solution_cfg_string2obj(in_str);
	solution_cfg_save();
	return 0;
}

int solution_handle_recovery_config(char *out_str)
{
	solution_cfg_load_default_config();

	char *config_str = solution_cfg_obj2string();
	strcpy(out_str, config_str);
	free(config_str);
	return 0;
}

int solution_handle_param_set(SOLUTION_PARAM_E type, char* val, unsigned int length)
{
	solution_handle_t *handle = g_solution_handle;
	if (handle == NULL || handle->impl == NULL || handle->impl->param_set == NULL)
		return -1;
	return handle->impl->param_set(type, val, length);
}

int solution_handle_param_get(SOLUTION_PARAM_E type, char* val, unsigned int* length)
{
	solution_handle_t *handle = g_solution_handle;
	if (handle == NULL || handle->impl == NULL || handle->impl->param_get == NULL)
		return -1;
	return handle->impl->param_get(type, val, length);
}
