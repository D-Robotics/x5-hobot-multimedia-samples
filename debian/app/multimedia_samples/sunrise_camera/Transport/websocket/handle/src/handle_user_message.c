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

static int ws_get_stream_index(int key, int streams[], int stream_count) {
	for (int i = 0; i < stream_count; i++) {
		if (streams[i] == key) {
			return i + 1;
		}
	}
	// 如果未找到匹配的 key，返回 0 表示未找到
	return 0;
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
static int ws_send_h265_shm_stream_to_wfs(ws_client *ws_clt, shm_stream_t *shm_source, unsigned char *data, unsigned int *nalu_len){
#if 0
	frame_info info;
	unsigned int length = 0;
	unsigned int frame_size = 0;

	if (shm_stream_front(shm_source, &info, &data, &length) == 0) {
		NALU_t nalu;
		frame_size = length;

		int ret = get_annexb_nalu(data + *nalu_len, frame_size - *nalu_len, &nalu, 1);
		if (ret < 0) {
			SC_LOGE("[%s][%d] shm_source: %p data: %p length: %u *nalu_len: %d readers:%d",
					__func__, __LINE__, shm_source, data, length, *nalu_len, shm_stream_readers(shm_source));
			*nalu_len = 0;
			return 0;
		}
		if (ret > 0) *nalu_len += nalu.len + nalu.startcodeprefix_len;    //记录nalu偏移总长

		//只发送sps pps i p nalu, 其他抛弃
		if ( nalu.nal_unit_type == 1 || nalu.nal_unit_type == 32 ||
			nalu.nal_unit_type == 33 || nalu.nal_unit_type == 34 ||
			nalu.nal_unit_type == 19)
		{
			frame_size = nalu.len;

			// 发送数据, 需要发送带头信息的数据给 wfs
			ws_send_nalu_to_wfs(ws_clt, ws_get_stream_index(info.key, ws_clt->stream_chn, ws_clt->stream_count), info.pts,
				nalu.buf - nalu.startcodeprefix_len, nalu.len + nalu.startcodeprefix_len);
			if (nalu.nal_unit_type == 1 || nalu.nal_unit_type == 19)
			{
				*nalu_len = 0;
				int remains = shm_stream_remains(shm_source);
				if(remains > 10)
					SC_LOGI("shm_source:%p, framer video pts:%llu length:%d frame_size:%d remains:%d",
						shm_source, info.pts, length, frame_size, remains);

				//该帧发送完毕，包括sps pps等nalu拆分完毕，可以释放
				shm_stream_post(shm_source);
			}
		}
		else {
			SC_LOGW("shm_source:%p recv no support type: %d.", nalu.nal_unit_type);
			*nalu_len = 0;
			shm_stream_post(shm_source);
		}
	}else{
		usleep(20);
	}
#else
	frame_info info;
	unsigned int length = 0;
	unsigned int frame_size = 0;
	if (shm_stream_front(shm_source, &info, &data, &length) == 0) {
		frame_size = length;
		shm_stream_post(shm_source);
	}
#endif
	return frame_size;
}


static int ws_send_h264_shm_stream_to_wfs(ws_client *ws_clt, shm_stream_t *shm_source, unsigned char *data, unsigned int *nalu_len)
{

	frame_info info;
	unsigned int length = 0;
	unsigned int frame_size = 0;

	if (shm_stream_front(shm_source, &info, &data, &length) == 0) {
		NALU_t nalu;
		frame_size = length;
		int ret = get_annexb_nalu(data + *nalu_len, frame_size - *nalu_len, &nalu, 0);
		if (ret < 0) {
			int remains = shm_stream_remains(shm_source);

			SC_LOGE("shm_source [%s] data: %p length: %u *nalu_len: %d readers:%d, remains:%d.",
					 shm_source->name, data, length, *nalu_len, shm_stream_readers(shm_source), remains);
			*nalu_len = 0;
			shm_stream_post(shm_source);
			return -1;
		}

		if (ret > 0){ //记录nalu偏移总长
			*nalu_len += nalu.len + nalu.startcodeprefix_len;
		}
		// SC_LOGI("nal_unit_type:%d data:%p buf:%p len:%u, *nalu_len:%d, %d\n", nalu.nal_unit_type, data,
		// 			nalu.buf, nalu.len, *nalu_len, length);

		//只发送sps pps i p nalu, 其他抛弃
		if (nalu.nal_unit_type == 7 || nalu.nal_unit_type == 8
			|| nalu.nal_unit_type == 1 || nalu.nal_unit_type == 5)
		{
			frame_size = nalu.len;

			//在使用 nalu 内存前做如下检测
			//码流数据在大压力的情况下，可能会被覆盖，所以此处判断web 发送的数据范围是否合法
			//数据被覆盖后，发送出去也没问题，但是要保证程序不会奔溃
			unsigned char *encode_data_stream_start = nalu.buf - nalu.startcodeprefix_len;
			int32_t encode_data_stream_len = nalu.len + nalu.startcodeprefix_len;
			ret = nalu_is_beyond_source_data_range(encode_data_stream_start, encode_data_stream_len, data, length, shm_source->name);
			if(ret != 0){
				SC_LOGE("shm_source [%s] data range is error, so ignore this pkt.", shm_source->name);
				*nalu_len = 0;
				shm_stream_post(shm_source);
				return -1;
			}
			// 发送数据, 需要发送带头信息的数据给 wfs
			ws_send_nalu_to_wfs(ws_clt, ws_get_stream_index(info.key, ws_clt->stream_chn, ws_clt->stream_count), info.pts,
				encode_data_stream_start, encode_data_stream_len);
			if (nalu.nal_unit_type == 1 || nalu.nal_unit_type == 5)
			{
				*nalu_len = 0;
				int remains = shm_stream_remains(shm_source);
				if(remains > 10)
					SC_LOGI("shm_source [%s], framer video pts:%llu length:%d frame_size:%d remains:%d",
						shm_source->name, info.pts, length, frame_size, remains);

				//该帧发送完毕，包括sps pps等nalu拆分完毕，可以释放
				shm_stream_post(shm_source);
			}
			// 会出现只有 7 和 8 类型的包
			else if ((nalu.nal_unit_type == 7 || nalu.nal_unit_type == 8) && length == nalu.len + 4) {
				*nalu_len = 0;
				//该帧发送完毕，包括sps pps等nalu拆分完毕，可以释放
				shm_stream_post(shm_source);
			}else{
				SC_LOGD("recv nal_type: %d, so not post .\n", nalu.nal_unit_type);
			}
		}
		// 调试过程中遇到出现 type == 23 的情况，不解析直接抛弃掉
		else {
			*nalu_len = 0;
			shm_stream_post(shm_source);
		}
	}
	return frame_size;
}
static int ws_send_mjpeg_shm_stream_to_wfs(ws_client *ws_clt, shm_stream_t *shm_source, unsigned char *data, unsigned int *nalu_len)
{
	return 0;
}

static void *ws_push_stream_thread(void *ptr)
{
	tsThread *privThread = (tsThread*)ptr;
	ws_client *ws_clt = (ws_client*)privThread->pvThreadData;

	unsigned char* data[32] = {NULL};
	unsigned int nalu_len[32] = {0};

	SC_LOGI("thread [ws_push_stream_thread] start .");
	// 设置线程名，方便知道退出的是什么线程
	mThreadSetName(privThread, __func__);

	while (privThread->eState == E_THREAD_RUNNING) {
		int ret = 0;
		// 从共享内存中读取码流数据
		for (int i = 0; i < ws_clt->stream_count; i++) {
			if(ws_clt->codec_type == T_SDK_RTSP_VIDEO_TYPE_H264){
				ret |= ws_send_h264_shm_stream_to_wfs(ws_clt, ws_clt->shm_source[i], data[i], &nalu_len[i]);
			}else if(ws_clt->codec_type == T_SDK_RTSP_VIDEO_TYPE_H265) {
				ret |= ws_send_h265_shm_stream_to_wfs(ws_clt, ws_clt->shm_source[i], data[i], &nalu_len[i]);
			}else if(ws_clt->codec_type == T_SDK_RTSP_VIDEO_TYPE_MJPEG){
				ret |= ws_send_mjpeg_shm_stream_to_wfs(ws_clt, ws_clt->shm_source[i], data[i], &nalu_len[i]);
			}else{
				//do no nothing;
			}
		}

		if(ret == 0){ // 没有读取到数据, 延迟10ms 防止线程空转导致CPU 100%
			usleep(1 * 1000);
		}
	}
	for (int i = 0; i < ws_clt->stream_count; i++) {
		if (ws_clt->shm_source[i] != NULL) {
			shm_stream_destory(ws_clt->shm_source[i]); // 销毁共享内存读句柄
			ws_clt->shm_source[i] = NULL;
		}
	}
	SC_LOGI("thread [ws_push_stream_thread] end .");
	mThreadFinish(privThread);
	return NULL;
}

static int _do_start_stream(ws_client *ws_clt)
{
	int ret = 0;
	int i = 0;
	T_SDK_VENC_INFO venc_chn_info;
	int num_enabled_channels = 0;

	// 遍历所有通道
	for (i = 0; i < 32; i++) {
		if (ws_clt->venc_chns_status & (1 << i)) {
			ws_clt->stream_chn[num_enabled_channels++] = i;
			// 如果已找到最多 channel_count 个使能的通道，则退出循环
			if (num_enabled_channels >= ws_clt->stream_count) {
				break;
			}
		}
	}
	for (i = 0; i < num_enabled_channels; i++) {
		venc_chn_info.channel = ws_clt->stream_chn[i];
		ret = SDK_Cmd_Impl(SDK_CMD_VPP_VENC_CHN_PARAM_GET, (void*)&venc_chn_info);
		if(ret < 0)
		{
			SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_VENC_CHN_PARAM_GET Error, ERRCODE: %d\n", ret);
			return -1;
		}

		SC_LOGI("venc chn %d id %s, type: %d, frameRate: %d, stream_buf_size: %d",
			venc_chn_info.channel,
			venc_chn_info.enable == 1 ? "enable" : "disable", venc_chn_info.type,
			venc_chn_info.framerate, venc_chn_info.stream_buf_size);

		char shm_id[32] = {0}, shm_name[32] = {0};
		int type = venc_chn_info.type;
		sprintf(shm_id, "ws%d_id_%s_chn%d", ws_clt->socket_id, type == 96 ? "h264" :
									(type == 265 ? "h265" :
									(type == 26) ? "jpeg" : "other"), venc_chn_info.channel);
		sprintf(shm_name, "name_%s_chn%d", type == 96 ? "h264" :
									(type == 265 ? "h265" :
									(type == 26) ? "jpeg" : "other"), venc_chn_info.channel);

		if (type == 96){
			ws_clt->codec_type = T_SDK_RTSP_VIDEO_TYPE_H264;
			ws_clt->codec_type_string = "h264";
		}else if(type == 265){
			ws_clt->codec_type = T_SDK_RTSP_VIDEO_TYPE_H265;
			ws_clt->codec_type_string = "h265";
		}else if(type == 26){
			ws_clt->codec_type = T_SDK_RTSP_VIDEO_TYPE_MJPEG;
			ws_clt->codec_type_string = "jpeg";
		}else{
			ws_clt->codec_type_string = "h264";
			ws_clt->codec_type = T_SDK_RTSP_VIDEO_TYPE_H264;
			SC_LOGE("not support codec type [%d], so use default type :h264.", type);
		}

		ws_clt->shm_source[i] = shm_stream_create(shm_id, shm_name,
			STREAM_MAX_USER, venc_chn_info.suggest_buffer_item_count,
			venc_chn_info.suggest_buffer_region_size,
			SHM_STREAM_READ, SHM_STREAM_MALLOC);

		SC_LOGI("video_stream_create => shm_id: %s, shm_name: %s, max user: %d, framerate: %d, \
				stream_buf_size: %d bitrate:%d region size:%d, item count %d.",
			shm_id, shm_name, STREAM_MAX_USER,
			venc_chn_info.framerate, venc_chn_info.stream_buf_size, venc_chn_info.bitrate,
			venc_chn_info.suggest_buffer_region_size, venc_chn_info.suggest_buffer_item_count);

		if (ws_clt->shm_source[i] != NULL) {
			printf("shm_source is successfully created\n");
		} else {
			printf("Failed to create shm_source\n");
			return -1;
		}
	}

	memset(&ws_clt->stream_thread, 0, sizeof(tsThread));
	ws_clt->stream_thread.pvThreadData = (void*)ws_clt;
	mThreadStart(ws_push_stream_thread, &ws_clt->stream_thread, E_THREAD_JOINABLE);
	return 0;
}

static int _do_add_sms(int channel)
{
	int ret = 0;
	T_SDK_VENC_INFO venc_chn_info;
	// 从camera模块获取chn0的码流配置
	venc_chn_info.channel = channel; // venc 的 channel 需要作为入参，这个应该通过camera模块的参数get出来，这里先写死好了
	ret = SDK_Cmd_Impl(SDK_CMD_VPP_VENC_CHN_PARAM_GET, (void*)&venc_chn_info);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_VENC_CHN_PARAM_GET Error, ERRCODE: %d", ret);
		return -1;
	}

	SC_LOGI("venc chn %d id %s, type: %d, frameRate: %d\n", venc_chn_info.channel,
		venc_chn_info.enable == 1 ? "enable" : "disable", venc_chn_info.type,
		venc_chn_info.framerate);

	T_SDK_RTSP_SRV_PARAM sms_param = { 0 };
	int type = venc_chn_info.type;
	char *codec_type_string = "h264";

	sms_param.audio.enable = 0;
	sms_param.video.enable = 1;

	if (type == 96){
		sms_param.video.type = T_SDK_RTSP_VIDEO_TYPE_H264;
		codec_type_string = "h264";
	}else if(type == 265){
		sms_param.video.type = T_SDK_RTSP_VIDEO_TYPE_H265;
		codec_type_string = "h265";
	}else if(type == 26){
		sms_param.video.type = T_SDK_RTSP_VIDEO_TYPE_MJPEG;
		codec_type_string = "jpeg";
	}else{
		SC_LOGE("not support codec type [%d], so exit.", type);
		sms_param.video.type = T_SDK_RTSP_VIDEO_TYPE_H264;
		codec_type_string = "h264";
	}

	sprintf(sms_param.prefix, "stream_chn%d.%s", venc_chn_info.channel, codec_type_string);

	sprintf(sms_param.shm_id, "rtsp_id_%s_chn%d", codec_type_string, venc_chn_info.channel);
	sprintf(sms_param.shm_name, "name_%s_chn%d", codec_type_string, venc_chn_info.channel);
	sms_param.stream_buf_size = venc_chn_info.stream_buf_size;
	sms_param.video.framerate = venc_chn_info.framerate;

	sms_param.suggest_buffer_item_count = venc_chn_info.suggest_buffer_item_count;
	sms_param.suggest_buffer_region_size = venc_chn_info.suggest_buffer_region_size;

	SC_LOGI("prefix: %s, port: %d, video_framerate: %d, shm_id: %s, shm_name: %s, stream_buf_size: %d, region size %d, item count %d.",
		sms_param.prefix, sms_param.port,
		sms_param.video.framerate,
		sms_param.shm_id, sms_param.shm_name, sms_param.stream_buf_size,
		sms_param.suggest_buffer_region_size, sms_param.suggest_buffer_item_count);

	ret = SDK_Cmd_Impl(SDK_CMD_RTSP_SERVER_ADD_SMS, (void*)&sms_param);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_RTSP_SERVER_START Error, ERRCODE: %d", ret);
		return -1;
	}
	return ret;
}

int handle_user_msg(ws_list *ws_lst, ws_client *ws_clt, char *msg)
{
	int i, ret = 0;
	cJSON *root = cJSON_Parse(msg);
	cJSON *print_json = NULL;
	WS_CMD_KIND cmd_kind = WS_CMD_UNDEFINE;
	char cmd_context[WS_MAX_BUFFER] = {0};
	char ws_msg[WS_MAX_BUFFER + 64] = {0};
	int stream_chn_count = -1;
	unsigned int venc_chns_status = 0;

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
			// 1. 先stop、反初始化vin 、isp、vps、 venc 和 rtps 删除sms
			SC_LOGI("========================== DEL SMS ==========================");
			SDK_Cmd_Impl(SDK_CMD_RTSP_SERVER_DEL_SMS, NULL);

			SC_LOGI("==================== STOP VPP SOLUTION ======================");
			SDK_Cmd_Impl(SDK_CMD_VPP_STOP, NULL);

			SC_LOGI("==================== UNINIT VPP SOLUTION ====================");
			SDK_Cmd_Impl(SDK_CMD_VPP_UNINIT, NULL);

			// 2. 更新配置结构体
			SC_LOGI("================= SET VPP SOLUTION ====================");
			SDK_Cmd_Impl(SDK_CMD_VPP_SET_SOLUTION_CONFIG, (void *)cmd_context);
			print_json = cJSON_Parse(cmd_context);
			SC_LOGI("%s", cJSON_Print(print_json));
			free(print_json);

			// 3. 开始启动应用
			SC_LOGI("================= INIT VPP SOLUTION ====================");
			ret = SDK_Cmd_Impl(SDK_CMD_VPP_INIT, NULL);
			if(ret < 0)
			{
				SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_INIT Error, ERRCODE: %d", ret);
				ws_send_respose(ws_lst, ws_clt, "{\"kind\":1,\"app_status\": \"请检查sensor是否连接正常\"}");
				return -1;
			}

			usleep(500*1000);

			SC_LOGI("================= START VPP SOLUTION ====================");
			ret = SDK_Cmd_Impl(SDK_CMD_VPP_START, NULL);
			if(ret < 0)
			{
				SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_START Error, ERRCODE: %d", ret);
				ws_send_respose(ws_lst, ws_clt, "{\"kind\":1,\"app_status\": \"请检查sensor是否连接正常\"}");
				return -1;
			}

			usleep(500*1000);

			// 根据编码通道的配置添加推流
			SC_LOGI("================= ADD SMS ====================");
			SDK_Cmd_Impl(SDK_CMD_VPP_GET_VENC_CHN_STATUS, (void*)&venc_chns_status);
			SC_LOGI("venc_chns_status: 0x%x, ", venc_chns_status);
			for (i = 0; i < 32; i++) {
				if (venc_chns_status & (1 << i))
					_do_add_sms(i); // 给对应的编码数据建立rtsp推流sms
			}
			ws_send_respose(ws_lst, ws_clt, "{\"kind\":1,\"Status\":\"200\"}");
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
			// 避免重复拉流
			if (ws_clt->shm_source[stream_chn_count-1])
				break;

			SC_LOGI("start ws venc stream for %d channels", stream_chn_count);
			// 根据编码通道的配置添加推流
			SC_LOGI("================= START Websocket Video Stream ====================");
			SDK_Cmd_Impl(SDK_CMD_VPP_GET_VENC_CHN_STATUS, (void*)&venc_chns_status);
			SC_LOGD("venc_chns_status: %u", venc_chns_status);
			ws_clt->stream_count = stream_chn_count;
			ws_clt->venc_chns_status = venc_chns_status;
			ret = _do_start_stream(ws_clt);
			if (ret < 0) {
				SC_LOGE("start websocket push stream failed");
			}
			break;
		case WS_CMD_STOP_STREAM:
			SC_LOGI("================= Stop Websocket Video Stream ====================");
			SC_LOGI("stop ws venc stream for %d channels", cJSON_GetObjectItem(root, "param")->valueint);
			mThreadStop(&ws_clt->stream_thread);
			break;
		case WS_CMD_SYNC_TIME:
			SC_LOGD("sync pc time to : %d", cJSON_GetObjectItem(root, "param")->valueint);
			long int pc_t = cJSON_GetObjectItem(root, "param")->valueint;
// #if __GLIBC_MINOR__ == 31
			struct timespec res;
			res.tv_sec = pc_t;
			clock_settime(CLOCK_REALTIME,&res);
// #else
// 			stime(&pc_t);
// #endif
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
