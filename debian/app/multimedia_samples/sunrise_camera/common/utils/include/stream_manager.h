#ifndef STREAM_MANAGER_H
#define STREAM_MANAGER_H

#include "lock_utils.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct
{
	char 	name[64];
	void* 	addr;
	int 	size;
	int		ref_count;
}shmmap_node;

typedef enum{
	SHM_STREAM_READ = 1,
	SHM_STREAM_WRITE,
	SHM_STREAM_WRITE_BLOCK,
}SHM_STREAM_MODE_E;

typedef enum{
	SHM_STREAM_MMAP = 1,
	SHM_STREAM_MALLOC,
}SHM_STREAM_TYPE_E;

typedef struct
{
	int type;
	int key;
	int seq;
	unsigned int 		length;
	unsigned long long 	pts;
	unsigned int		t_time;
	unsigned int		framerate;	// or samplerate
	unsigned int		width;
	unsigned int		height;
	char reserved[12];
}frame_info;

typedef int (*shm_stream_info_callback)(frame_info info, unsigned char* data, unsigned int length);

typedef struct
{
	char			id[32];	//	用户标识
	unsigned int	index;	//	当前读写info_array下标
	unsigned int	offset;	//	当前数据存储偏移 只用作写模式
	unsigned int	users;	//	读用户数
	shm_stream_info_callback	callback;
}shm_user_t;

typedef enum{
	DATA_ACCESS_STATUS_IDEL = 0,
	DATA_ACCESS_STATUS_ACCESSING,
}SHM_STREAM_DATA_ACCESS_STATUS_E;

typedef struct
{
	unsigned int	offset;		//	数据存储偏移
	unsigned int	lenght;		//	数据长度
	SHM_STREAM_DATA_ACCESS_STATUS_E access_status; // 当前是否正在读取
	frame_info		info;		//	数据info
}shm_info_t;

typedef struct
{
//private
	char*	user_array;			//	用户数组初始地址
	char*	info_array;			//	信息数组初始地址
	char*	base_addr;			//	数据初始地址
	CMtx	mtx;
	char	name[20];			//	创建的共享文件名
	unsigned int index;			//	用户在userArray中的下标
	unsigned int max_frames;	//	最大的帧缓冲数，主要是frameArray数组元素个数
	unsigned int max_users;
	unsigned int size;
	SHM_STREAM_MODE_E mode;
	SHM_STREAM_TYPE_E type;
	unsigned int info_count;
}shm_stream_t;


//public
shm_stream_t* shm_stream_create(char* id, const char* name, int users, int infos, int size, SHM_STREAM_MODE_E mode, SHM_STREAM_TYPE_E type);
void shm_stream_destory(shm_stream_t* handle);

int shm_stream_put(shm_stream_t* handle, frame_info info, unsigned char* data, unsigned int length);
int shm_stream_get(shm_stream_t* handle, frame_info* info, unsigned char** data, unsigned int* length);
int shm_stream_front(shm_stream_t* handle, frame_info* info, unsigned char** data, unsigned int* length);
int shm_stream_post(shm_stream_t* handle);
int shm_stream_sync(shm_stream_t* handle);
int shm_stream_remains(shm_stream_t* handle);
int shm_stream_readers(shm_stream_t* handle);
int shm_stream_info_callback_register(shm_stream_t* handle, shm_stream_info_callback callback);
int shm_stream_info_callback_unregister(shm_stream_t* handle);
int shm_stream_is_already_create(char* id, char* name, int max_users);
//private
void* shm_stream_malloc(shm_stream_t* handle, const char* name, unsigned int size);
int   shm_stream_malloc_fix(shm_stream_t* handle, char* id, const char* name, int users, void* addr);
void  shm_stream_unmalloc(shm_stream_t* handle);
void* shm_stream_mmap(shm_stream_t* handle, const char* name, unsigned int size);
void  shm_stream_unmap(shm_stream_t* handle);

#ifdef __cplusplus
}
#endif

#endif
