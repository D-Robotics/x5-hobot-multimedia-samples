#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <stdint.h>
#include <pthread.h>
#ifdef __cplusplus
extern "C"{
#endif

#include <assert.h>

#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif

#define TRUE	1
#define FALSE	0

//执行控制台指令
int exec_cmd(const char *cmd);
int exec_cmd_ex(const char *cmd, char* res, int max);
//执行控制台指令, 判断执行结果是否存在str字符串
int exec_cmd_chstr_exist(char* cmd, char* str);
//获取系统剩余内存
unsigned long get_system_mem_freeKb();
//获取目录剩余存储空间
unsigned long long get_system_tf_freeKb(char* dir);
//获取系统自启动时间到现在的tick数
int32_t get_tick_count();
//从全路径文件名中获取文件名
void  get_file_pure_name(char* full_name,char * dest);
//使用select延时
void select_delay_ms(int nMillisecond);
//判断文件/文件夹是否存在
int is_file_exist(const char* file_path);
int is_dir_exist(const char* dir_path);

int int2hex2str(char *pValue,int lValue,int lCharLen);
int int2str(char *pValue,int lValue,int lCharLen);
//字符串翻转
char* strrev(char* s);
//范围随机数
int random_range(int min, int max);

void localtime_string(char* tstr);

//创建4M栈大小的线程
int pthread_create_4m(pthread_t *pt_id, void *(*proc)(void *), void* arg);
int get_addr_ip(char* demain, char* ip, int socktype);
int str_splite(char* str, char* split, char* des, int rows, int row_size);

//打印文件内容到标准输出
void print_file(const char *file_name);

void delete_files_with_extension(const char *dir_path, const char *file_extension);

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_array)	(sizeof(_array) / sizeof(_array[0]))
#endif

#ifdef __cplusplus
}
#endif

#endif