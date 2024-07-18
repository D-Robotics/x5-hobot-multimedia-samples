#ifndef SOLUTION_HANDLE_H
#define SOLUTION_HANDLE_H

#include "solution_struct_define.h"
#include "communicate/sdk_common_struct.h"

#include "vp_wrap.h"

typedef struct vpp_ops {
	int (*init_param)(void); // 初始化VIN、VSE、VENC、BPU等模块的配置参数
	int (*init)(void); // sdk 初始化，根据配置初始化
	int (*uninit)(void); // 反初始化
	int (*start)(void); // 启动媒体相关的各个模块
	int (*stop)(void); // 停止
	// 本模块支持的CMD都通过以下两个接口简直实现
	int (*param_set)(SOLUTION_PARAM_E type, char* val, unsigned int length);
	int (*param_get)(SOLUTION_PARAM_E type, char* val, unsigned int* length);
} vpp_ops_t;

int solution_handle_init(void);
int solution_handle_uninit(void);
int solution_handle_start(void);
int solution_handle_stop(void);
int solution_handle_get_config(char *out_str);
int solution_handle_set_config(char *in_str);
int solution_handle_save_config(char *in_str);
int solution_handle_recovery_config(char *out_str);
int solution_handle_param_set(SOLUTION_PARAM_E type, char* val, unsigned int length);
int solution_handle_param_get(SOLUTION_PARAM_E type, char* val, unsigned int* length);

#endif