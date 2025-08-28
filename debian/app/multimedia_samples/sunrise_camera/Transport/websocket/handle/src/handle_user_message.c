#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "communicate/sdk_common_cmd.h"
#include "communicate/sdk_common_struct.h"
#include "communicate/sdk_communicate.h"

#include "utils/nalu_utils.h"
#include "utils/mthread.h"
#include "utils/utils_log.h"
#include "utils/common_utils.h"
#include "utils/stream_define.h"
#include "utils/stream_manager.h"
#include "utils/cJSON.h"
#include "utils/time_utils.h"

#include "Handshake.h"
#include "Errors.h"
#include "handle_user_message.h"
#include "Communicate.h"
#include "WebsocketWrap.h"

typedef enum {
	WS_CMD_UNDEFINE = -1,
	WS_CMD_HEARTBEAT,
	WS_CMD_SWITCH_SOLUTION,
	WS_CMD_SNAP,
	WS_CMD_START_STREAM,
	WS_CMD_STOP_STREAM,
	WS_CMD_SYNC_TIME,
	WS_CMD_SET_BITRATE,
	WS_CMD_GET_CONFIG,
	WS_CMD_SAVE_CONFIG,
	WS_CMD_RECOVERY_CONFIG,
} WS_CMD_KIND;

void ws_send_respose(ws_list *ws_lst, ws_client *ws_clt, char *msg)
{
	ws_message *m = message_new();
	m->len = strlen(msg);
	m->msg = malloc(sizeof(char)*(m->len+1) );
	memset(m->msg, 0, m->len+1);
	memcpy(m->msg, msg, m->len);
	if ( (encodeMessage(m)) != CONTINUE) {
		message_free(m);
		free(m);
		return;
	}
	list_multicast_one(ws_lst, ws_clt, m);
	message_free(m);
	free(m);
}

void save_video_data2file_for_debug(char* name, int nal_unit_type, unsigned char*data, int length){
	static FILE *enc_data_file = NULL;
	if(enc_data_file == NULL){
		if(nal_unit_type == 32){
				char enc_file_name [100];
				sprintf(enc_file_name, "/tmp/front_websocker_%s.h265", name);

				enc_data_file = fopen(enc_file_name, "wb");
				if(enc_data_file == NULL){
					SC_LOGE("open file %s failed.", (char *)enc_file_name);
				}
		}else{
			printf("ignore nalu type [%d], before idr.\n", nal_unit_type);
		}
	}
	if(enc_data_file != NULL){
		size_t elementsWritten = fwrite((unsigned char*)data,
			1, length, enc_data_file);
		if (elementsWritten != length) {
			SC_LOGE("write websocker file failed, return %d.", elementsWritten);
		}
	}
}
void handle_error_respose_msg(char *ws_msg, int ws_msg_len, T_SDK_CHECK_INFO *check_info, const char *config_str){

	if(check_info->ion_lack != 0){
		snprintf(ws_msg, ws_msg_len,
			"{\"kind\":1,\"app_status\": \"配置失败: ION内存不足, 缺少%dB, 点击确定恢复配置\", \"solution_configs\": %s}",
			check_info->ion_lack, config_str);
	}else if(check_info->vpu_lack != 0.0){
		snprintf(ws_msg, ws_msg_len,
			"{\"kind\":1,\"app_status\": \"配置失败: 视频编解码处理器能力不足, 点击确定恢复配置\", \"solution_configs\": %s}",
			config_str);
	}else if(check_info->decode_param_check_info.not_match_count != 0){
		int offset = snprintf(ws_msg, ws_msg_len, "{\"kind\":1,\"app_status\": \"配置失败: 解码参数配置错误, 点击确定恢复配置 \",");
		offset += snprintf(ws_msg + offset, ws_msg_len - offset, "\"detailed\": [");
		for (int i = 0; i < check_info->decode_param_check_info.not_match_count; i++) {
			T_SDK_DECODE_PARAM_CHECK_SINGLE_INFO *decode_param_info = &check_info->decode_param_check_info.decode_params[i];

			if(strcmp(decode_param_info->actual_codec_type, "error") == 0){
				offset += snprintf(ws_msg + offset, ws_msg_len - offset,
								"\"第%d路 指定的文件打开失败\"", decode_param_info->pipeline_id + 1);
			}else if(strcmp(decode_param_info->actual_codec_type, "unsupport") == 0){
				offset += snprintf(ws_msg + offset, ws_msg_len - offset,
								"\"第%d路 指定的文件无法识别编码格式\"", decode_param_info->pipeline_id + 1);
			}else{
				offset += snprintf(ws_msg + offset, ws_msg_len - offset,
								"\"第%d路 指定文件是%s, 选择解码类型是%s\"", decode_param_info->pipeline_id + 1,
								decode_param_info->actual_codec_type, decode_param_info->config_codec_type);
			}

			if (i < check_info->decode_param_check_info.not_match_count - 1) {
				offset += snprintf(ws_msg + offset, ws_msg_len - offset, ",");
			}
		}
		offset += snprintf(ws_msg + offset, ws_msg_len - offset, "]");
		snprintf(ws_msg + offset, ws_msg_len - offset, ",\"solution_configs\": %s}", config_str);
	}else{
		SC_LOGE("should not run here.");
		exit(-1);
	}
}

int handle_user_msg(ws_list *ws_lst, ws_client *ws_clt, char *msg)
{
	int ret = 0;
	cJSON *root = cJSON_Parse(msg);
	cJSON *print_json = NULL;
	WS_CMD_KIND cmd_kind = WS_CMD_UNDEFINE;
	char cmd_context[WS_MAX_BUFFER] = {0};
	char ws_msg[WS_MAX_BUFFER + 64] = {0};
	int stream_chn_count = -1;
	unsigned int venc_chns_status = 0;
	int check_param_is_error = 0;
	T_SDK_CHECK_INFO check_info;

	if (root == NULL) return -1;

	SC_LOGD("handle_user_msg: %s\n", cJSON_Print(root));

	cmd_kind = cJSON_GetObjectItem(root, "kind")->valueint;

	switch (cmd_kind)
	{
		case WS_CMD_HEARTBEAT:
			// do nothing
			break;
		case WS_CMD_SWITCH_SOLUTION:
			strcpy(cmd_context, cJSON_GetObjectItem(root, "param")->valuestring);
			print_json = cJSON_Parse(cmd_context);
			SC_LOGI("%s", cJSON_Print(print_json));
			free(print_json);

			// 1. 先stop、反初始化vin 、isp、vps、 venc 和 rtps 删除sms
			SC_LOGI("========================== DEL SMS ==========================");
			SDK_Cmd_Impl(SDK_CMD_RTSP_SERVER_DEL_SMS, NULL);

			SC_LOGI("==================== STOP VPP SOLUTION ======================");
			SDK_Cmd_Impl(SDK_CMD_VPP_STOP, NULL);

			SC_LOGI("==================== UNINIT VPP SOLUTION ====================");
			SDK_Cmd_Impl(SDK_CMD_VPP_UNINIT, NULL);

			//放到stop pipeline 的后面
			SC_LOGI("================= CHECK VPP SOLUTION ====================");
			check_info.param = cmd_context;
			check_info.ion_lack = 0;
			check_info.vpu_lack = 0.0;
			check_info.decode_param_check_info.not_match_count = 0;
			SDK_Cmd_Impl(SDK_CMD_VPP_CHECK_SOLUTION_CONFIG, (void *)&check_info);

			if((check_info.ion_lack != 0) || (check_info.vpu_lack != 0.0) ||
				(check_info.decode_param_check_info.not_match_count != 0)){
				// 2. 不更新配置结构体（上传错误信息）
				check_param_is_error = 1;
				SC_LOGW("solution param check failed: [ion_lack:%d] [vpu_lack:%f]\
					[decode param error count %d], so ignore this config.",
					check_info.ion_lack, check_info.vpu_lack, check_info.decode_param_check_info.not_match_count);
			}else{
				// 2. 更新配置结构体
				SC_LOGI("================= SET VPP SOLUTION ====================");
				SDK_Cmd_Impl(SDK_CMD_VPP_SET_SOLUTION_CONFIG, (void *)cmd_context);
			}
			// 3. 开始启动应用
			SC_LOGI("================= INIT VPP SOLUTION ====================");
			ret = SDK_Cmd_Impl(SDK_CMD_VPP_INIT, NULL);
			if(ret < 0)
			{
				SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_INIT Error, ERRCODE: %d", ret);
				ws_send_respose(ws_lst, ws_clt, "{\"kind\":1,\"app_status\": \"请检查sensor是否连接正常\"}");
				exit(-1);
			}

			usleep(500*1000);

			SC_LOGI("================= START VPP SOLUTION ====================");
			ret = SDK_Cmd_Impl(SDK_CMD_VPP_START, NULL);
			if(ret < 0)
			{
				SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_START Error, ERRCODE: %d, so exit(-1)", ret);
				ws_send_respose(ws_lst, ws_clt, "{\"kind\":1,\"app_status\": \"请检查sensor是否连接正常\"}");
				exit(-1);
			}

			usleep(500*1000);


			if(check_param_is_error){
				//获取当前的配置，传递给网页端
				char config_str[WS_MAX_BUFFER] = {0};
				SDK_Cmd_Impl(SDK_CMD_VPP_GET_SOLUTION_CONFIG, (void *)config_str);
				memset(ws_msg, '\0', sizeof(ws_msg));
				handle_error_respose_msg(ws_msg, sizeof(ws_msg), &check_info, config_str);

				SC_LOGI("Not support current config, so send old config to web: %s", ws_msg);
				ws_send_respose(ws_lst, ws_clt, ws_msg);

			}else{
				ws_send_respose(ws_lst, ws_clt, "{\"kind\":1,\"Status\":\"200\"}");
			}
			break;
		case WS_CMD_SNAP:
			cJSON *param_item = cJSON_GetObjectItemCaseSensitive(root, "param");
			if (!cJSON_IsObject(param_item)) {
				SC_LOGE("WS_CMD_SNAP: Invalid param received");
				return -1;
			}

			cJSON *type_item = cJSON_GetObjectItemCaseSensitive(param_item, "type");
			cJSON *format_item = cJSON_GetObjectItemCaseSensitive(param_item, "format");
			cJSON *videoNum_item = cJSON_GetObjectItemCaseSensitive(param_item, "videoNum");

			if (!cJSON_IsString(type_item) || !cJSON_IsString(format_item) || !cJSON_IsString(videoNum_item)) {
				SC_LOGE("WS_CMD_SNAP: Invalid type or format received");
				return -1;
			}

			const char *type = type_item->valuestring;
			const char *format = format_item->valuestring;
			// video_id 代表web上的第几个 video 控件，从1开始计数
			// 需要结合当前使能了多少路pipeline来获取到对应的 pipeline id
			int32_t video_id = atoi(videoNum_item->valuestring);

			SC_LOGI("WS_CMD_SNAP type: %s, format: %s, videoNum: %d", type, format, video_id);

			if (strcmp(type, "vin") == 0 && strcmp(format, "raw") == 0) {
				ret = SDK_Cmd_Impl(SDK_CMD_VPP_GET_RAW_FRAME, (void *)&video_id);
			} else if (strcmp(type, "isp") == 0 && strcmp(format, "yuv") == 0) {
				ret = SDK_Cmd_Impl(SDK_CMD_VPP_GET_ISP_FRAME, (void *)&video_id);
			} else if (strcmp(type, "vse") == 0 && strcmp(format, "yuv") == 0) {
				ret = SDK_Cmd_Impl(SDK_CMD_VPP_GET_VSE_FRAME, (void *)&video_id);
			} else {
				SC_LOGE("WS_CMD_SNAP: Undefined command");
			}

			if (ret < 0) {
				SC_LOGE("SDK_Cmd_Impl Error, ERRCODE: %d", ret);
				return -1;
			}
			break;
		case WS_CMD_START_STREAM:
			stream_chn_count = cJSON_GetObjectItem(root, "param")->valueint;
			SC_LOGI("start ws venc stream for %d channels", stream_chn_count);
			// 根据编码通道的配置添加推流
			SC_LOGI("================= START Websocket Video Stream ====================");
			SDK_Cmd_Impl(SDK_CMD_VPP_GET_VENC_CHN_STATUS, (void*)&venc_chns_status);
			SC_LOGD("venc_chns_status: %u", venc_chns_status);
			break;
		case WS_CMD_STOP_STREAM:
			SC_LOGI("================= Stop Websocket Video Stream ====================");
			SC_LOGI("stop ws venc stream for %d channels", cJSON_GetObjectItem(root, "param")->valueint);

			break;
		case WS_CMD_SYNC_TIME:
			SC_LOGD("sync pc time to : %d", cJSON_GetObjectItem(root, "param")->valueint);
			long int pc_t = cJSON_GetObjectItem(root, "param")->valueint;

			struct timespec res;
			res.tv_sec = pc_t;
			clock_settime(CLOCK_REALTIME,&res);
			break;
		case WS_CMD_GET_CONFIG:
		{
			// 获取场景配置
			SC_LOGI("================= GetConfig ====================");
			memset(ws_msg, '\0', sizeof(ws_msg));
			char config_str[WS_MAX_BUFFER] = {0};
			SDK_Cmd_Impl(SDK_CMD_VPP_GET_SOLUTION_CONFIG, (void *)config_str);
			sprintf(ws_msg, "{\"kind\":%d,\"solution_configs\": %s}", WS_CMD_GET_CONFIG, config_str);
			SC_LOGI("Send Config: %s", ws_msg);
			ws_send_respose(ws_lst, ws_clt, ws_msg);

			break;
		}
		case WS_CMD_SET_BITRATE:
		{
			int bitrate = cJSON_GetObjectItem(root, "param")->valueint;
			SC_LOGD("bitrate = %d", bitrate);
			SDK_Cmd_Impl(SDK_CMD_VPP_VENC_BITRATE_SET, (void*)&bitrate);
			break;
		}
		case WS_CMD_SAVE_CONFIG:
		{
			SC_LOGI("Save vpp solution config");
			char cfg_str[WS_MAX_BUFFER] = {0};
			strcpy(cfg_str, cJSON_GetObjectItem(root, "param")->valuestring);
			print_json = cJSON_Parse(cfg_str);
			SC_LOGI("%s", cJSON_Print(print_json));
			free(print_json);
			SDK_Cmd_Impl(SDK_CMD_VPP_SAVE_SOLUTION_CONFIG, (void *)cfg_str);
			break;
		}
		case WS_CMD_RECOVERY_CONFIG:
		{
			memset(ws_msg, '\0', sizeof(ws_msg));
			char config_str[WS_MAX_BUFFER] = {0};
			SDK_Cmd_Impl(SDK_CMD_VPP_RECOVERY_SOLUTION_CONFIG, (void *)config_str);
			sprintf(ws_msg, "{\"kind\":%d,\"solution_configs\": %s}", WS_CMD_GET_CONFIG, config_str);
			SC_LOGD("ws_msg: %s", ws_msg);
			ws_send_respose(ws_lst, ws_clt, ws_msg);
			break;
		}
		case WS_CMD_UNDEFINE:
		default:
			SC_LOGE("WS cmder undefined");
	}

	if (root)
		cJSON_free(root);

	return 0;
}
