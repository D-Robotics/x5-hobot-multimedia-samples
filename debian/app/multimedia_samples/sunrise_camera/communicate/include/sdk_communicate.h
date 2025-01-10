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
