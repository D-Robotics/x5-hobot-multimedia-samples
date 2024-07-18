/**************************************************************************
 * 	FileName:		gos_log.c
 *	Description:	日志
 *	Copyright(C):	2014-2020 gos Inc.
 *	Version:		V 1.0
 *	Author:			Chenjb
 *	Created:		2014-06-09
 *	Updated:
 *
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "utils_log.h"
#ifdef __cplusplus
extern "C"{
#endif

static char s_log_buffer[MAX_LOG_BUFSIZE] = {0};
static pthread_mutex_t s_buffer_mtx = PTHREAD_MUTEX_INITIALIZER;

static log_ctrl* s_log_ctrl = NULL;
static int s_log_level = LOG_INFO;

log_ctrl* log_ctrl_instance_create(char* file, int level, int wt)
{
	if(s_log_ctrl == NULL)
	{
		s_log_ctrl = log_ctrl_create(file, level, wt);
	}

	return s_log_ctrl;
}

log_ctrl* log_ctrl_create(char* file, int level, int wt)
{
	log_ctrl* log = (log_ctrl*)malloc(sizeof(log_ctrl));

	log->fd = fopen(file, "a");
	if(log->fd == NULL)
	{
		free(log);
		printf("log_ctrl_create error to open file %s\n", file);
		return NULL;
	}

	strcpy(log->file, file);
	log->level = level;
	log->wt = wt;

	return log;
}

void log_ctrl_destory(log_ctrl* log)
{
	if(log->fd != NULL)
		fclose(log->fd);

	free(log);
}
int  log_ctrl_level_set(log_ctrl* log, int level)
{
	if(log != NULL)
	{
		log->level = level;
	}
	else if(s_log_ctrl != NULL)
	{
		s_log_ctrl->level = level;
	}
	else
	{
		s_log_level = level;
	}

	return 0;
}
int log_ctrl_level_get(log_ctrl* log)
{
	if (log != NULL) {
		return log->level;
	} else if (s_log_ctrl != NULL) {
		return s_log_ctrl->level;
	} else {
		return s_log_level;
	}
}
int  log_ctrl_wt_set(log_ctrl* log,int wt)
{
	if(log != NULL)
	{
		log->wt = wt;
	}
	else if(s_log_ctrl != NULL)
	{
		s_log_ctrl->wt = wt;
	}

	return 0;
}

int log_ctrl_file_copy(log_ctrl* log)
{
	char buff[1024];
	int len;
	FILE *in,*out;
	char bak[128] = {0};

	snprintf(bak, sizeof(bak), "%.123s.bak", log->file);
	in = fopen(log->file,"r+");
	if(in == NULL)
		return -1;

	out = fopen(bak,"w+");
	if(out == NULL)
	{
		fclose(in);
		return -1;
	}

	while((len = fread(buff,1,sizeof(buff),in)) != 0)
	{
		fwrite(buff,1,len,out);
	}

	fclose(out);
	fclose(in);

	return 0;
}

int log_ctrl_file_write(log_ctrl* log, char* data, int len)
{
	if(log == NULL)
	{
		return -1;
	}
	else if(log->fd == NULL)
	{
		log->fd = fopen(log->file, "a+");
	}

	fwrite(data, 1, len, log->fd);
	fflush(log->fd);

	//检测文件大小 是否备份刷新
	int fd = fileno(log->fd);
	unsigned long filesize = 0;
	struct stat statbuff;
	if(fstat(fd, &statbuff) >= 0)
	{
		filesize = statbuff.st_size;
	}

	if(filesize > MAX_LOG_FILESIZE)
	{
		fclose(log->fd);
		int ret = log_ctrl_file_copy(log);
		if(ret == 0)
			log->fd = fopen(log->file, "w+");
		else
			log->fd = fopen(log->file, "a+");
	}

	return 0;
}

int  log_ctrl_print(log_ctrl* log, int level, const char* t, ...)
{
	log_ctrl* ctrl = NULL;
	if(log != NULL)
	{
		ctrl = log;
	}
	else if(s_log_ctrl != NULL)
	{
		ctrl = s_log_ctrl;
	}

	if(ctrl != NULL)
	{
		if(level <= ctrl->level)
		{
			if(ctrl->wt == 0)
			{
				struct timeval v;
				gettimeofday(&v, 0);
				struct tm *p = localtime(&v.tv_sec);
				char fmt[256] = {0}; //限制t不能太大
				sprintf(fmt, "%s%04d/%02d/%02d %02d:%02d:%02d.%03d %s %s\n"CNONE,level==LOG_TRACE? "":(level==LOG_DEBUG? LIGHT_GREEN:(level==LOG_INFO? LIGHT_CYAN:(level==LOG_WARN?YELLOW:LIGHT_RED)))
						, 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, (int)(v.tv_usec/1000)
						, level==LOG_TRACE? "TRACE":(level==LOG_DEBUG? "DEBUG":(level==LOG_INFO? "!INFO":(level==LOG_WARN? "!WARN":"ERROR"))), t);

				va_list params;
				va_start(params, t);
				vfprintf(stdout, fmt, params);
				va_end(params);
				fflush(stdout);
			}
			else
			{

				struct timeval v;
				gettimeofday(&v, 0);
				struct tm *p = localtime(&v.tv_sec);
				char fmt[256] = {0}; //限制t不能太大
				sprintf(fmt, "%04d/%02d/%02d %02d:%02d:%02d.%03d %s %s\n"
						, 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, (int)(v.tv_usec/1000)
						, level==LOG_TRACE? "TRACE":(level==LOG_DEBUG? "DEBUG":(level==LOG_INFO? "!INFO":(level==LOG_WARN? "!WARN":"ERROR"))), t);

				char fmt0[256] = {0}; //限制t不能太大
				sprintf(fmt0, "%s%04d/%02d/%02d %02d:%02d:%02d.%03d %s %s\n"CNONE,level==LOG_TRACE? "":(level==LOG_DEBUG? LIGHT_GREEN:(level==LOG_INFO? LIGHT_CYAN:(level==LOG_WARN?YELLOW:LIGHT_RED)))
						, 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, (int)(v.tv_usec/1000)
						, level==LOG_TRACE? "TRACE":(level==LOG_DEBUG? "DEBUG":(level==LOG_INFO? "!INFO":(level==LOG_WARN? "!WARN":"ERROR"))), t);

				//这里需要上锁
				va_list params;
				va_start(params, t);

				pthread_mutex_lock(&s_buffer_mtx);
				memset(s_log_buffer, 0, MAX_LOG_BUFSIZE);
				vsnprintf(s_log_buffer, MAX_LOG_BUFSIZE, fmt, params);
				log_ctrl_file_write(ctrl, s_log_buffer, strlen(s_log_buffer));
				pthread_mutex_unlock(&s_buffer_mtx);

				va_end(params);

				va_list params0;
				va_start(params0, t);
				vfprintf(stdout, fmt0, params0);
				va_end(params0);
				fflush(stdout);
			}
		}
	}
	else
	{
		if(level <= s_log_level)
		{
			struct timeval v;
			gettimeofday(&v, 0);
			struct tm *p = localtime(&v.tv_sec);
			char fmt[256] = {0}; //限制t不能太大
			sprintf(fmt, "%s%04d/%02d/%02d %02d:%02d:%02d.%03d %s %s\n"CNONE,level==LOG_TRACE? "":(level==LOG_DEBUG? LIGHT_GREEN:(level==LOG_INFO? LIGHT_CYAN:(level==LOG_WARN?YELLOW:LIGHT_RED)))
					, 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, (int)(v.tv_usec/1000)
					, level==LOG_TRACE? "TRACE":(level==LOG_DEBUG? "DEBUG":(level==LOG_INFO? "!INFO":(level==LOG_WARN? "!WARN":"ERROR"))), t);

			va_list params;
			va_start(params, t);
			vfprintf(stdout, fmt, params);
			va_end(params);
			fflush(stdout);
		}

		return 0;
	}

	return 0;
}
#ifdef __cplusplus
}
#endif

