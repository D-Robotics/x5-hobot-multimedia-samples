#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/vfs.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "common_utils.h"

#ifdef __cplusplus
extern "C"{
#endif

int exec_cmd(const char *cmd)
{
	FILE *fp = NULL;
	if((fp = popen(cmd, "r")) == NULL)
	{
		printf("Fail to popen\n");
		return -1;
	}
	pclose(fp);

	return 0;
}

int exec_cmd_ex(const char *cmd, char* res, int max)
{
	if(cmd == NULL || res == NULL || max <= 0)
		return -1;

	FILE *pp = popen(cmd, "r");
	if(!pp)
	{
		printf("error, cannot popen cmd: %s\n", cmd);
		return -1;
	}

	int length;
	char tmp[1024] = {0};

	length = max;
	if(max > 1024) length = 1024;

	while(fgets(tmp, length, pp) != NULL)
	{
		strcat(res, tmp);
	}

	pclose(pp);

	return strlen(res);
}

int exec_cmd_chstr_exist(char* cmd, char* str)
{
	if(NULL == str || NULL == cmd)
	{
		return -1;
	}

	FILE *fp = NULL;
	char buf[256]= {0};

	if((fp = popen(cmd, "r")) == NULL)
	{
		printf("Fail to popen\n");
		return -1;
	}

	while(fgets(buf, sizeof(buf), fp) != NULL)
	{
		if(strstr(buf, str))
		{
			pclose(fp);
			return 0;
		}
	}
	pclose(fp);

	return -1;
}

unsigned long long get_system_tf_freeKb(char* dir)
{
	int ret;
	if(dir == NULL) return 0;

	struct statfs diskInfo;
	ret = statfs(dir, &diskInfo);
	if(ret == 0)
	{
		unsigned long long totalBlocks = diskInfo.f_bsize;
		unsigned long long freeDisk = diskInfo.f_bfree*totalBlocks;
		return freeDisk/1024LL;
	}

	return 0;
}

unsigned long get_system_mem_freeKb()
{
	int error;
	struct sysinfo s_info;

	error = sysinfo(&s_info);
	if(error == 0)
	{
		return s_info.freeram/1024L;
	}
	return 0;
}

int32_t get_tick_count()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (int32_t)now.tv_sec;
}

void  get_file_pure_name(char* full_name,char * dest)
{
	char*  mn_first = full_name;
	char*  mn_last  = full_name + strlen( full_name );
	if ( strrchr( full_name, '\\' ) != NULL )
		mn_first = strrchr( full_name, '\\' ) + 1;
	else if ( strrchr( full_name, '/' ) != NULL )
		mn_first = strrchr( full_name, '/' ) + 1;
	//	if ( strrchr( full_name, '.' ) != NULL )
	//		mn_last = strrchr( full_name, '.' );
	if ( mn_last < mn_first )
		mn_last = full_name + strlen( full_name );

	memmove(dest ,mn_first,(mn_last-mn_first));
}

void select_delay_ms(int nMillisecond)
{
	struct	 timeval   mTimeOut;	 //定时的时间
	mTimeOut.tv_sec   =   nMillisecond/1000;
	mTimeOut.tv_usec   =   (nMillisecond%1000)*1000;

	select(0,	NULL,	NULL,	NULL,	&mTimeOut);
}

int is_file_exist(const char* file_path)
{
	if(file_path == NULL)
		return -1;
	if(access(file_path, F_OK) == 0)
		return 0;

	return -1;
}

int is_dir_exist(const char* dir_path)
{
	if(dir_path == NULL)
		return -1;

	DIR *dirptr = NULL;
	if((dirptr = opendir(dir_path)) == NULL)
		return -1;

	closedir(dirptr);

	return 0;
}

int int2hex2str(char *pValue,int lValue,int lCharLen)
{
	char tmp[10];
	memset(tmp,0,sizeof(tmp));
	sprintf(tmp,"%%0%dx",lCharLen);
	return sprintf(pValue,tmp,lValue);
}

int int2str(char *pValue,int lValue,int lCharLen)
{
	char tmp[10];
	memset(tmp,0,sizeof(tmp));
	sprintf(tmp,"%%0%dd",lCharLen);
	return sprintf(pValue,tmp,lValue);
}

char* strrev(char* s)
{
	/* h指向s的头部 */
	char* h = s;
	char* t = s;
	char ch;

	/* t指向s的尾部 */
	while(*t++){};
	t--;    /* 与t++抵消 */
	t--;    /* 回跳过结束符'\0' */

	/* 当h和t未重合时，交换它们所指向的字符 */
	while(h < t)
	{
		ch = *h;
		*h++ = *t;    /* h向尾部移动 */
		*t-- = ch;    /* t向头部移动 */
	}

	return(s);
}

int random_range(int min, int max)
{
	int pos, dis;

	srand((int)time(NULL));
	if(min == max)
	{
		return max;
	}
	else if(max > min)
	{
		pos = min;
		dis = max - min + 1;
		return rand() % dis + pos;
	}
	else
	{
		pos = max;
		dis = min - max + 1;
		return rand() % dis + pos;
	}
}

void localtime_string(char* tstr)
{
	char *wday[]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	time_t timep;
	struct tm *p;

	tzset();
	time(&timep);
	p=localtime(&timep);

	sprintf(tstr,"%d/%02d/%02d %s %02d:%02d:%02d", (1900+p->tm_year),(1+p->tm_mon), p->tm_mday
			, wday[p->tm_wday],p->tm_hour, p->tm_min, p->tm_sec);
}

int pthread_create_4m(pthread_t* pt_id, void *(*proc)(void *), void* arg)
{
	pthread_attr_t attr;

	pthread_attr_init (&attr);
	int stacksize = (4 << 10 ) << 10;
	pthread_attr_setstacksize(&attr, stacksize);

	int ret = pthread_create(pt_id,&attr,proc,arg);
	if (ret != 0)
	{
		pthread_attr_destroy (&attr);
		printf("pthread_create error %s\n", strerror(ret));
		return -1;
	}
	pthread_attr_destroy (&attr);

	return 0;
}

int get_addr_ip(char* demain, char* ip, int socktype)
{
	struct addrinfo hints;
	struct addrinfo *res, *cur;
	struct sockaddr_in *addr;
	int ret;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET; /* Allow IPv4 */
	hints.ai_flags = AI_PASSIVE; /* For wildcard IP address */
	hints.ai_protocol = 0; /* Any protocol */
	hints.ai_socktype = socktype;//SOCK_DGRAM;
	ret = getaddrinfo(demain, NULL,&hints,&res);
	if (ret < 0) {
		//		printf("getaddrinfo error\n");
		return -1;
	}

	ret = -1;
	for (cur = res; cur != NULL; cur = cur->ai_next) {
		addr = (struct sockaddr_in *)cur->ai_addr;
		if(cur->ai_family == AF_INET && cur->ai_socktype == socktype)
		{
			inet_ntop(AF_INET, &addr->sin_addr, ip, 16);
			ret = 0;
		}
	}
	freeaddrinfo(res);
	return ret;
}

/*
//不适用与两个;;合并在一起，如果合并在一起，中间的空字符将被忽略
char dest[15][100];
int size = str_splite("84;57;43;47;58;57;57;45;65;75;57;", ";", (char*)dest, 15, 100);
*/
int str_splite(char* str, char* split, char* des, int rows, int row_size)
{
	if(str == NULL || split == NULL)
		return -1;

	char *outer_ptr = NULL;
	char* buf = (char*)malloc(strlen(str) + 1);
	strncpy(buf, str, strlen(str) + 1);

	int count = 0;
	char*p = strtok_r(buf, split, &outer_ptr);
	while(p!=NULL)
	{
		snprintf(des+row_size*count, row_size, "%s", p);
		p = strtok_r(NULL, split, &outer_ptr);

		count++;
		if(count > rows)
			break;
	}
	free(buf);

	return count;
}

void print_file(const char *file_name)
{
	if(NULL == file_name)
	{
		return;
	}

	FILE *fp;
	if( !(fp = fopen( file_name, "r" )) )
	{
		fprintf(stderr,"Cannot open file %s.", file_name);
		return;
	}
	int c;
	while( (c = fgetc(fp) ) != EOF )
	{
		fputc(c, stdout);
	}
	fclose(fp);
}


// 删除指定目录下特定后缀的文件
void delete_files_with_extension(const char *dir_path, const char *file_extension)
{
	DIR *dir;
	struct dirent *entry;

	dir = opendir(dir_path);
	if (!dir) {
		printf("[EEROR] Failed to open directory: %s\n", dir_path);
		return;
	}

	size_t ext_len = strlen(file_extension);

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_REG) { // 只处理普通文件
			const char *filename = entry->d_name;
			size_t filename_len = strlen(filename);

			if (filename_len > ext_len && strncmp(filename + filename_len - ext_len, file_extension, ext_len) == 0) {
				// 如果文件名以指定的后缀结尾
				char filepath[PATH_MAX];
				snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filename);
				if (remove(filepath) != 0) {
					printf("[ERROR] Failed to delete file: %s\n", filepath);
				} else {
					printf("[INFO] Deleted file: %s\n", filepath);
				}
			}
		}
	}

	closedir(dir);
}


#ifdef __cplusplus
}
#endif


