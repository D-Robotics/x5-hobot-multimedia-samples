#ifndef   _UTILS_SYSLOG_H_
#define   _UTILS_SYSLOG_H_

#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C"{
#endif

//宏定义
#define MAX_LOG_BUFSIZE		2048
#define MAX_LOG_FILESIZE	100*1024


#define   LOG_EMERG   0
#define   LOG_ERR     1
#define   LOG_WARN    2
#define   LOG_INFO    3
#define   LOG_DEBUG   4
#define   LOG_TRACE   5

//打印字体颜色
#define CNONE         "\033[m"
#define RED          "\033[0;32;31m"
#define LIGHT_RED    "\033[1;31m"
#define GREEN        "\033[0;32;32m"
#define LIGHT_GREEN  "\033[1;32m"
#define BLUE         "\033[0;32;34m"
#define LIGHT_BLUE   "\033[1;34m"
#define DARY_GRAY    "\033[1;30m"
#define CYAN         "\033[0;36m"
#define LIGHT_CYAN   "\033[1;36m"
#define PURPLE       "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN        "\033[0;33m"
#define YELLOW       "\033[1;33m"
#define LIGHT_GRAY   "\033[0;37m"
#define WHITE        "\033[1;37m"

typedef struct log_ctrl_s
{
	FILE*	fd;
	char	file[128];
	int		level;
	int		wt;
}log_ctrl;

log_ctrl* log_ctrl_instance_create(char* file, int level, int wt);
log_ctrl* log_ctrl_create(char* file, int level, int wt);
void log_ctrl_destory(log_ctrl* log);
int  log_ctrl_level_set(log_ctrl* log, int level);
int log_ctrl_level_get(log_ctrl* log);
int  log_ctrl_wt_set(log_ctrl* log,int wt);
int  log_ctrl_file_write(log_ctrl* log, char* data, int len);
int  log_ctrl_print(log_ctrl* log, int level, const char* t, ...);

// 以下宏定义中的 "[%s][%04d]" t "" 的t前后需要加空格，否则编译的时候会报以下error
// utils_log.h:60:74: error: unable to find string literal operator ‘operator""t’
// with ‘const char [11]’, ‘long unsigned int’ arguments
#define SC_LOGT(t, ...) 	log_ctrl_print(NULL, LOG_TRACE, "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SC_LOGD(t, ...) 	log_ctrl_print(NULL, LOG_DEBUG, "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SC_LOGI(t, ...) 	log_ctrl_print(NULL, LOG_INFO,  "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SC_LOGW(t, ...) 	log_ctrl_print(NULL, LOG_WARN,  "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SC_LOGE(t, ...) 	log_ctrl_print(NULL, LOG_ERR,   "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SC_LOGM(t, ...) 	log_ctrl_print(NULL, LOG_EMERG, "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define SC_CLOGT(c, t, ...) 	log_ctrl_print(c, LOG_TRACE, "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SC_CLOGD(c, t, ...) 	log_ctrl_print(c, LOG_DEBUG, "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SC_CLOGI(c, t, ...) 	log_ctrl_print(c, LOG_INFO,  "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SC_CLOGW(c, t, ...) 	log_ctrl_print(c, LOG_WARN,  "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SC_CLOGE(c, t, ...) 	log_ctrl_print(c, LOG_ERR,   "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SC_CLOGM(c, t, ...) 	log_ctrl_print(c, LOG_EMERG, "[%s][%04d]" t "", __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define SC_ERR_CON_EQ(ret, a, str) do { \
	if ((ret) != (a)) { \
		log_ctrl_print(NULL, LOG_ERR, "[%s][%04d] %s failed, ret(%d)", __FUNCTION__, __LINE__, str, (int32_t)(ret)); \
		return (ret); \
	} \
} while(0)

#ifdef __cplusplus
}
#endif

#endif

