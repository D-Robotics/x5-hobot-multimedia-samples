#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils/utils_log.h"
#include "mk_mediakit.h"
static int media_server_is_inited = 0;

const static int media_server_codec_type(const char *codec_name)
{
	if (strcmp(codec_name, "h264") == 0)
	{
		return MKCodecH264;
	}
	else if (strcmp(codec_name, "h265") == 0)
	{
		return MKCodecH265;
	}
	else if (strcmp(codec_name, "jpeg") == 0)
	{
		return MKCodecJPEG;
	}
	else
	{
		// 如果输入的编码器名称未知，可以返回一个错误代码或默认值
		return -1; // 返回-1表示未知编码器
	}
}

void API_CALL on_mk_http_request(const mk_parser parser,
                                 const mk_http_response_invoker invoker,
                                 int *consumed,
                                 const mk_sock_info sender) {
    const char *url = mk_parser_get_url(parser);
    *consumed = 1;

    // ✅ 1. 安全解析请求路径（处理根目录 `/` → 返回 `/index.html`）

	const char *api_root_dir_key = "api.downloadRoot";
    const char *base_dir = mk_get_option(api_root_dir_key);
	if(base_dir == NULL){
		SC_LOGE("not found root directory form config key:%s\n", api_root_dir_key);
		const char *headers_404[] = {"Content-Type", "text/plain", NULL};
        mk_http_response_invoker_do(invoker, 404, headers_404, mk_http_body_from_string("404 Not Found", 13));
        return;
	}

    // ⚠️ 关键修复：如果路径是 `/`，默认返回 `/index.html`
	char safe_path[256] = {0};
    if (strcmp(url, "/") == 0) {
        snprintf(safe_path, sizeof(safe_path), "%s/index.html", base_dir);
    } else {
        snprintf(safe_path, sizeof(safe_path), "%s%s", base_dir, url);
    }

	// ✅ 2. 检查文件是否存在
    struct stat file_stat;
    if (stat(safe_path, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
        // 🔇 关键修改：如果是 .map 文件，静默返回 404（不打印日志）
        if (strstr(url, ".map") != NULL) {
            const char *headers_404[] = {"Content-Type", "text/plain", NULL};
            mk_http_response_invoker_do(invoker, 404, headers_404, mk_http_body_from_string("", 0));
            return;
        }
        // 其他文件正常打印 404 日志
        SC_LOGE("File not found: %s\n", safe_path);
        const char *headers_404[] = {"Content-Type", "text/plain", NULL};
        mk_http_response_invoker_do(invoker, 404, headers_404, mk_http_body_from_string("404 Not Found", 13));
        return;
    }


    // ✅ 3. 打开文件并读取内容
    FILE *file = fopen(safe_path, "rb");
    if (!file) {
        SC_LOGE("Failed to open file: %s\n", safe_path);
        const char *headers_500[] = {"Content-Type", "text/plain", NULL};
        mk_http_response_invoker_do(invoker, 500, headers_500, mk_http_body_from_string("500 Internal Server Error", 24));
        return;
    }

    // ✅ 4. 读取文件内容
    char *file_data = malloc(file_stat.st_size);
    size_t bytes_read = fread(file_data, 1, file_stat.st_size, file);
    fclose(file);

    if (bytes_read != file_stat.st_size) {
        free(file_data);
        SC_LOGE("Failed to fully read file: %s\n", safe_path);
        const char *headers_500[] = {"Content-Type", "text/plain", NULL};
        mk_http_response_invoker_do(invoker, 500, headers_500, mk_http_body_from_string("500 Internal Server Error", 24));
        return;
    }

    // ✅ 5. 动态设置 Content-Type
    const char *content_type = "application/octet-stream";
    if (strstr(url, ".wasm") != NULL) {
        content_type = "application/wasm";
    } else if (strstr(url, ".js") != NULL) {
        content_type = "application/javascript";
    } else if (strstr(url, ".html") != NULL || strcmp(url, "/") == 0) {
        content_type = "text/html";  // ✅ 根目录请求也返回 HTML 类型
    } else if (strstr(url, ".css") != NULL) {
        content_type = "text/css";
    } else if (strstr(url, ".map") != NULL) {  // ✅ 新增 .map 文件支持
        content_type = "application/json";     // SourceMap 实际上是 JSON 格式
    }

    // ✅ 6. 构造响应头（支持跨域和缓存）
    const char *response_headers[] = {
        "Content-Type", content_type,
        "Access-Control-Allow-Origin", "*",
        "Cache-Control", "public, max-age=3600",
        NULL
    };

    // ✅ 7. 发送 HTTP 响应
    mk_http_body body = mk_http_body_from_string(file_data, bytes_read);
    mk_http_response_invoker_do(invoker, 200, response_headers, body);
    mk_http_body_release(body);
    free(file_data);
}


void API_CALL on_mk_media_no_reader(const mk_media_source sender)
{
	// SC_LOGI("on_mk_media_no_reader");
}

int media_server_init(char *config_path)
{

	if (media_server_is_inited)
	{
		SC_LOGW("meida server is already inited, so return .");
		return 0;
	}

	media_server_is_inited = 1;
	mk_config config;
	config.ini = config_path;
	config.ini_is_path = 1;
	config.log_level = 0;
	config.log_mask = LOG_FILE;
	config.log_file_path = NULL;
	config.log_file_days = 0;
	config.ssl = NULL;
	config.ssl_is_path = 1;
	config.ssl_pwd = NULL;
	config.thread_num = 1;
	mk_env_init(&config);

	const char *rtsp_port = mk_get_option("rtsp.port");
	const char *http_port = mk_get_option("http.port");
	const char *rtc_port = mk_get_option("rtc.port");
	if (http_port == NULL || rtc_port == NULL || rtsp_port == NULL)
	{
		SC_LOGE("http_port %s、rtc_port %s、rtsp_port %s no find", http_port, rtc_port, rtsp_port);
		return -1;
	}

	uint16_t http_port_int, rtsp_port_int;
	// uint16_t rtc_port_int;
	http_port_int = mk_http_server_start(atoi(http_port), 0);
	rtsp_port_int = mk_rtsp_server_start(atoi(rtsp_port), 0);
	// rtc_port_int = mk_rtc_server_start(atoi(rtc_port));
	if ((http_port_int == 0) ||
		// (rtc_port_int == 0) ||
		(rtsp_port_int == 0))
	{
		SC_LOGE("http rtsp rtc server start failed. %d %d %d",
				http_port_int,
				// rtc_port_int,
				rtsp_port_int);
		return -1;
	}
	mk_events events = {
		.on_mk_media_changed = NULL,
		.on_mk_media_publish = NULL,
		.on_mk_media_play = NULL,
		.on_mk_media_not_found = NULL,
		.on_mk_media_no_reader = on_mk_media_no_reader,
		.on_mk_http_request = on_mk_http_request,
		.on_mk_http_access = NULL,
		.on_mk_http_before_access = NULL,
		.on_mk_rtsp_get_realm = NULL,
		.on_mk_rtsp_auth = NULL,
		.on_mk_record_mp4 = NULL,
		.on_mk_shell_login = NULL,
		.on_mk_flow_report = NULL};
	mk_events_listen(&events);

	mk_tcp_session_events events_server = {
		.on_mk_tcp_session_create = NULL,
		.on_mk_tcp_session_data = NULL,
		.on_mk_tcp_session_manager = NULL,
		.on_mk_tcp_session_disconnect = NULL};
	mk_tcp_server_events_listen(&events_server);
	mk_tcp_server_start(8080, mk_type_ws);
	return 0;
}

// ch0/main
// ch1/sub
void *media_server_create_media(const char *meida_name, const char *stream_name, const char *codec_type_name)
{
#if 0
	SC_LOGI("hcy");
	mk_media meida_tmp = mk_media_create("__defaultVhost__", meida_name, stream_name, 0, 0, 0);
	if(meida_tmp == NULL){
		SC_LOGI("mk meida create failed. media_name %s, stream_name %s.", meida_name, stream_name);
		return NULL;
	}
#else
	mk_ini ini = mk_ini_create();
	mk_ini_set_option_int(ini, "modify_stamp", 0);
	mk_ini_set_option_int(ini, "enable_hls", 0);
	mk_ini_set_option_int(ini, "enable_mp4", 0);

	mk_media meida_tmp = mk_media_create2("__defaultVhost__", meida_name, stream_name, 0, ini);
	if (meida_tmp == NULL)
	{
		SC_LOGI("mk meida create failed. media_name %s, stream_name %s.", meida_name, stream_name);
		return NULL;
	}
	SC_LOGI("media server create meida: %s/%s", meida_name, stream_name);
#endif
	// create track
	codec_args v_args = {0};
	int codec_type = media_server_codec_type(codec_type_name);
	mk_track v_track = mk_track_create(codec_type, &v_args);
	if (v_track == NULL)
	{
		SC_LOGI("mk track create failed. media_name %s, stream_name %s.", meida_name, stream_name);
		return NULL;
	}
	mk_media_init_track(meida_tmp, v_track);

	mk_media_init_complete(meida_tmp);
	mk_track_unref(v_track);
	return meida_tmp;
}

int media_server_push_video(void *media, const char *data, int length, uint64_t pts, uint64_t dts, const char *codec_name)
{
	mk_media meida_tmp = (mk_media)media;

	int codec_type = media_server_codec_type(codec_name);
	mk_frame frame = mk_frame_create(codec_type, dts, pts, data, length, NULL, NULL);
	if (frame == NULL)
	{
		SC_LOGI("mk frame create failed.");
		return -1;
	}

	int ret = mk_media_input_frame(meida_tmp, frame);
	if (ret != 1)
	{
		SC_LOGI("mk frame input frame failed. %d", ret);
		return -1;
	}
	mk_frame_unref(frame);

	return 0;
}

void media_server_destroy_media(void *media)
{
	mk_media meida_tmp = (mk_media)media;
	mk_media_release(meida_tmp);
}
void media_server_uninit()
{

	mk_stop_all_server();
}