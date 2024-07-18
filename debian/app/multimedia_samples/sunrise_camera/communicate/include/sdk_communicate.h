#ifndef SDK_COMMUNICATE_H
#define SDK_COMMUNICATE_H

#include "sdk_common_cmd.h"

typedef int (*SDK_CMD_IMPL_CALL)(SDK_CMD_E cmd, void* param);
typedef struct
{
	SDK_CMD_E			cmd;
	SDK_CMD_IMPL_CALL	call;
	int					enable;
}sdk_cmd_reg_t;

#ifdef __cplusplus
extern "C"{
#endif
int sdk_globle_prerare();
int sdk_cmd_register(SDK_CMD_E cmd, SDK_CMD_IMPL_CALL call, int enable);
int sdk_cmd_unregister(SDK_CMD_E cmd);
int sdk_cmd_impl(SDK_CMD_E cmd, void* param);
#ifdef __cplusplus
}
#endif

#define SDK_Cmd_Impl sdk_cmd_impl
#define SDK_Globle_Prerare sdk_globle_prerare

#endif
