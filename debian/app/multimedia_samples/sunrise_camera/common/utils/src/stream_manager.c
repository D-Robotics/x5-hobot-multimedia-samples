#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "stream_manager.h"
#include "utils_log.h"
#include "lock_utils.h"
#include "cmap.h"

static cmap* s_shmmap = NULL;
static CMtx s_shmmap_lock = NULL;
/**
 * 1. 读写锁 只保护了 关键结构体(shm_info_t 和 shm_user_t), 防止读端访问了 错误的地址
 * 2. 读写锁 并没有保护数据，因为只有在异常情况(读端被卡住)才会出现写端覆盖读端数据的问题
 * 		并且数据被覆盖不会导致程序奔溃，并且读端被卡住 无论如何 数据都会丢弃，所以可以对数据加锁
 * 		如果对数据加锁，会有两种情况
 * 		a. 内存拷贝后，交给其他模块，比如 rtsp 发送： 增加了内存拷贝
 * 		b. 直接使用数据，等待数据使用完成， 比如等待 rtsp发送完成，增加了锁的保护时间
*/
static pthread_rwlock_t s_shm_rwlock;

// 如果要多个模块共享同一块内存，id要不一样，name、user、infos参数需要一样
// mode和type根据具体读写情况配置
shm_stream_t* shm_stream_create(char* id, const char* name, int users, int infos, int size, SHM_STREAM_MODE_E mode, SHM_STREAM_TYPE_E type)
{
	int i;
	shm_stream_t* handle = (shm_stream_t*)malloc(sizeof(shm_stream_t));
	memset(handle, 0, sizeof(shm_stream_t));

	//映射共享内存
	void* addr = NULL;
	if(type == SHM_STREAM_MMAP)
		addr = shm_stream_mmap(handle, name, users*sizeof(shm_user_t)+infos*sizeof(shm_info_t)+size);
	else if(type == SHM_STREAM_MALLOC)
	{
		addr = shm_stream_malloc(handle, name, users*sizeof(shm_user_t)+infos*sizeof(shm_info_t)+size);
		if(addr) shm_stream_malloc_fix(handle, id, name, users, addr);
	}
	if(addr == NULL)
	{
		SC_LOGE("shm_stream_mmap error");
		free(handle);
		return NULL;
	}

	handle->mtx = cmtx_create();
	handle->mode = mode;
	handle->type = type;
	handle->max_frames = infos;
	handle->max_users = users;
	handle->size = size;
	handle->user_array = (char*)addr;
	handle->info_array = handle->user_array + users*sizeof(shm_user_t);
	handle->base_addr  = handle->info_array + infos*sizeof(shm_info_t);
	handle->info_count = 0;
	snprintf(handle->name, 20, "%s", name);
	SC_LOGI("[%s] name:%s handle addr: %p, addr:%p, size: %d, users: %d, infos: %d",
		handle->name, id, handle, addr, size, users, infos);

	cmtx_enter(handle->mtx);
	shm_user_t* user = (shm_user_t*)handle->user_array;

	if(mode == SHM_STREAM_WRITE || mode == SHM_STREAM_WRITE_BLOCK)
	{
		handle->index = 0;
		//写模式默认使用user[0]
		user[0].index = 0;
		user[0].offset = 0;
		user[0].users = 0;

		snprintf(user[0].id, 32, "%s", id);
		for(i=1; i<users; i++)	//	初始化其他模式的读下标
		{
			if(strlen(user[i].id) != 0)
			{
				SC_LOGI("[CreateWriter]reader user[%d].id:%s", i, user[i].id);
				user[i].index = user[0].index;
				user[0].users++;
			}
		}
	}
	else
	{
		//如果是重复注册
		for (i=1; i<users; i++)
		{
			if (strncmp(user[i].id, id, 32) == 0)
			{
				handle->index = i;
				user[i].index = user[0].index;
				goto shm_stream_create_done;
			}
		}

		//查找空位插入  若超过users则会出现异常
		for (i=1; i<users; i++)
		{
			if (strlen(user[i].id) == 0)
			{
				handle->index = i;
				user[i].index = user[0].index;
				user[i].callback = NULL;
				user[0].users++;
				snprintf(user[i].id, 32, "%s", id);
				SC_LOGI("[CreateReader]reader user[%d].id:%s", i, user[i].id);

				break;
			}
		}
	}

	printf("[%s] show all reader [", handle->name);
	for(i=1; i<users; i++)	//	初始化其他模式的读下标
	{
		printf("%d=>%s, ", i, user[i].id);
	}
	printf("]\n");

shm_stream_create_done:
	cmtx_leave(handle->mtx);
	return handle;
}

void shm_stream_destory(shm_stream_t* handle)
{
	if(handle == NULL){
		SC_LOGE("handle is null.");
		return;
	}

	cmtx_enter(handle->mtx);
	shm_user_t* user = (shm_user_t *)handle->user_array;

	if(handle->mode == SHM_STREAM_READ)
		user[0].users--;

	SC_LOGI("[%s] name:%s handle addr: %p, index: %d, writer user count:%d",
		user[handle->index].id, handle->name, handle, handle->index, user[0].users);

	memset(user[handle->index].id, 0, 32);
	if(handle->type == SHM_STREAM_MMAP)
		shm_stream_unmap(handle);
	else if(handle->type == SHM_STREAM_MALLOC)
	{
		shm_stream_unmalloc(handle);
	}

	cmtx_leave(handle->mtx);;
	cmtx_delete(handle->mtx);

	if(handle != NULL)
		free(handle);
}

int shm_stream_readers_callback(shm_stream_t* handle, frame_info info, unsigned char* data, unsigned int length)
{
	if(handle == NULL) return -1;
	int i;
	shm_user_t* users = (shm_user_t*)handle->user_array;
	for(i=1; i<handle->max_users; i++)	//	初始化其他模式的读下标
	{
		if(strlen(users[i].id) != 0 && users[i].callback != NULL)
		{
			SC_LOGI("max_users:%d i:%d user[i].id:%s user[i].callback:%p", handle->max_users, i, users[i].id, users[i].callback);
			users[i].callback(info, data, length);
		}
	}

	return 0;
}

int shm_stream_info_callback_register(shm_stream_t* handle, shm_stream_info_callback callback)
{
	cmtx_enter(handle->mtx);
	shm_user_t* user = (shm_user_t*)handle->user_array;
	user[handle->index].callback = callback;
	cmtx_leave(handle->mtx);

	return 0;
}

int shm_stream_info_callback_unregister(shm_stream_t* handle)
{
	cmtx_enter(handle->mtx);
	shm_user_t* user = (shm_user_t*)handle->user_array;
	user[handle->index].callback = NULL;
	cmtx_leave(handle->mtx);

	return 0;
}


int shm_stream_put(shm_stream_t* handle, frame_info info, unsigned char* data, unsigned int length)
{
	if(handle == NULL) return -1;
	//如果没有人想要数据 则不put
	if(shm_stream_readers(handle) == 0){
		return -1;
	}
	cmtx_enter(handle->mtx);
	unsigned int head;
	shm_user_t* users = (shm_user_t*)handle->user_array;
	shm_info_t* infos = (shm_info_t*)handle->info_array;

	pthread_rwlock_wrlock(&s_shm_rwlock);
	head = users[0].index % handle->max_frames;
	memcpy(&infos[head].info, &info, sizeof(frame_info));
	infos[head].lenght = length;
	if(length + users[0].offset > handle->size){ 	//数据存储区不够存储了， 从头存储

		infos[head].offset = 0;
		users[0].offset = 0;
		if(handle->info_count < handle->max_frames){
			SC_LOGW("[%s] writer:%s data region is overflow, info max count is %d, current info index is %d, count is %d.",
				handle->name, users[0].id, handle->max_frames, head, handle->info_count);
		}else{
			SC_LOGD("[%s] writer:%s data region is normal, info max count is %d, current info index is %d, count is %d.",
				handle->name, users[0].id, handle->max_frames, head, handle->info_count);
		}
		handle->info_count = 0;
	}
	else{
		infos[head].offset = users[0].offset;
	}
	char* dst_data_addr = handle->base_addr+infos[head].offset;

	//生产者下次操作的位置
	users[0].offset += length;
	users[0].index = (users[0].index + 1 ) % handle->max_frames;

	//检测：消费者正在读取的数据区是否被生产者覆盖掉
	int checked_user_count = 0;
	for (int i = 1; i< handle->max_users; i++)
	{
		if (strlen(users[i].id) == 0){
			continue;
		}
		int reader_index = users[i].index % handle->max_frames;
		/*
			1. 消费者正在访问过程中
			2. 生成者覆盖了正在访问的位置
		*/

		// SC_LOGI("r:%d, w:%d", reader_index, users[0].index);
		if(reader_index == users[0].index){
			if(infos[reader_index].access_status == DATA_ACCESS_STATUS_ACCESSING){
				SC_LOGW("[%s] writer:%s covered reader:%s at index:%d, and reader is reading.",
								handle->name, users[0].id, users[i].id, reader_index);
			}else{
				SC_LOGI("[%s] writer:%s covered reader:%s at index:%d.",
								handle->name, users[0].id, users[i].id, reader_index);
			}
		}
		checked_user_count++;
		if(users[0].users == checked_user_count){
			break;
		}
	}

	// infos[head].access_status = DATA_ACCESS_STATUS_IDEL; //此处不应该更新访问状态
	/*
		数据拷贝要加锁保护场景分析：

		1. 写的过程中，被读走(读线程已经饥饿很久了，马上拷贝数据，但是数据正在拷贝的过程中)
			a. 触发场景：数据发送快的情况，很快发送完了，等待Info更
			b. 是否频繁：正常情况也会发生，比较频繁
			c. 如何处理：加锁，避免发生

		2. 读的过程中，被写覆盖 ：
			a. 触发场景：数据发送不过来，整个FIFO存满了数据， 新数据覆盖旧数据
			b. 是否频繁：在大压力的情况下，才会触发(数据发送速度跟不上，产生的速度)
			c. 如何处理：
				是否需要加锁：没必要 （代码改动大，且意义不大, 增加日志）
				处理方法：增加打印信息
	*/
	memcpy(dst_data_addr, data, length);
	handle->info_count++;
	pthread_rwlock_unlock(&s_shm_rwlock);

	cmtx_leave(handle->mtx);
	return 0;
}

int shm_stream_get(shm_stream_t* handle, frame_info* info, unsigned char** data, unsigned int* length)
{
	if(handle == NULL) return -1;

	unsigned int tail, head;

	cmtx_enter(handle->mtx);

	pthread_rwlock_rdlock(&s_shm_rwlock);
	shm_user_t* users = (shm_user_t*)handle->user_array;
	head = users[0].index % handle->max_frames;
	tail = users[handle->index].index % handle->max_frames;
//	SC_LOGI("users[0].index:%d users[handle->index].index:%d handle->index:%d head:%d tail:%d", users[0].index, users[handle->index].index, handle->index, head, tail);

	if (head != tail)
	{
		shm_info_t* infos = (shm_info_t*)handle->info_array;
		memcpy(info, &infos[tail].info, sizeof(frame_info));
		*data = (unsigned char*)(handle->base_addr + infos[tail].offset);
		*length = infos[tail].lenght;

		users[handle->index].index = (tail + 1 ) % handle->max_frames;
		pthread_rwlock_unlock(&s_shm_rwlock);
		cmtx_leave(handle->mtx);
		return 0;
	}
	else
	{
		*length = 0;
		pthread_rwlock_unlock(&s_shm_rwlock);
		cmtx_leave(handle->mtx);
		return -1;
	}
}

int shm_stream_front(shm_stream_t* handle, frame_info* info, unsigned char** data, unsigned int* length)
{
	if(handle == NULL) return -1;

	unsigned int tail, head;

	cmtx_enter(handle->mtx);
	pthread_rwlock_rdlock(&s_shm_rwlock);
	shm_user_t* users = (shm_user_t*)handle->user_array;
	head = users[0].index % handle->max_frames;
	tail = users[handle->index].index % handle->max_frames;
	/*SC_LOGI("handle: %p, handle->index:%d, head:%d tail:%d", handle, handle->index, head, tail);*/

	if (head != tail)
	{
		shm_info_t* infos = (shm_info_t*)handle->info_array;
		memcpy(info, &infos[tail].info, sizeof(frame_info));
		*data = (unsigned char*)(handle->base_addr + infos[tail].offset);
		/*SC_LOGI("handle->base_addr: %p, infos[tail].offset: %d", handle->base_addr, infos[tail].offset);*/
		*length = infos[tail].lenght;

		infos[tail].access_status = DATA_ACCESS_STATUS_ACCESSING;
		pthread_rwlock_unlock(&s_shm_rwlock);
		cmtx_leave(handle->mtx);
		return 0;
	}
	else
	{
		*length = 0;

		pthread_rwlock_unlock(&s_shm_rwlock);
		cmtx_leave(handle->mtx);
		return -1;
	}
	return 0;
}

/*
	1. 关于读写下标更新，实际能存储数据包的最大个数 < handle->max_frames 的情况分析：
		a. 问题：是否会出现 读者 访问 如下区间的数据： [实际存储数据包最大个数 , handle->max_frames]
		b. 分析：由于 实际能存储数据包的最大个数 < handle->max_frames 的情况 只会在选择 裸数据存储时，
			会做判断shm_stream_t.info_array 会按照最大个数 handle->max_frames 创建，
			并且不受 数据区 实际存储数据包最大个数的限制，所以没有问题

	2. 如何保证消费者 不会读到 旧的数据
		a. 生成者的下标：下一个将要写的位置
		b. 消费者：读之前 如果 读的位置是 生成者的下标（下一个将要写的位置），就返回
		c. 保证了 消费者的下标 永远 在 生成者 的前一个
*/
int shm_stream_post(shm_stream_t* handle)
{
	if(handle == NULL) return -1;

	unsigned int tail, head;

	cmtx_enter(handle->mtx);
	pthread_rwlock_rdlock(&s_shm_rwlock);
	shm_user_t* users = (shm_user_t*)handle->user_array;
	head = users[0].index % handle->max_frames;
	tail = users[handle->index].index % handle->max_frames;

	//更新正在读取的数据的状态
	shm_info_t* infos = (shm_info_t*)handle->info_array;
	infos[tail].access_status = DATA_ACCESS_STATUS_IDEL;

	if (head != tail)
	{
		users[handle->index].index = (tail + 1 ) % handle->max_frames;
	}
	pthread_rwlock_unlock(&s_shm_rwlock);
	cmtx_leave(handle->mtx);

	return 0;
}

int shm_stream_sync(shm_stream_t* handle)
{
	if(handle == NULL) return -1;

	unsigned int tail, head;
	shm_user_t *user;

	cmtx_enter(handle->mtx);
	user = (shm_user_t*)handle->user_array;
	head = user[0].index % handle->max_frames;
	tail = user[handle->index].index % handle->max_frames;
	if(head != tail)
	{
		user[handle->index].index = user[0].index % handle->max_frames;
	}
	cmtx_leave(handle->mtx);

	return 0;
}

int shm_stream_remains(shm_stream_t* handle)
{
	if(handle == NULL) return -1;

    int ret;
	cmtx_enter(handle->mtx);

	unsigned int tail, head;
	shm_user_t *user;

	user = (shm_user_t*)handle->user_array;
	head = user[0].index % handle->max_frames;
	tail = user[handle->index].index % handle->max_frames;

    ret = (head + handle->max_frames - tail)% handle->max_frames;

	cmtx_leave(handle->mtx);
	return ret;

}

int shm_stream_readers(shm_stream_t* handle)
{
	if(handle == NULL) return -1;

	int ret;
	cmtx_enter(handle->mtx);

	shm_user_t* user = (shm_user_t*)handle->user_array;
	ret = user[0].users;

	cmtx_leave(handle->mtx);
	return ret;
}

void* shm_stream_malloc(shm_stream_t* handle, const char* name, unsigned int size)
{
	if(handle == NULL) return NULL;

	if(s_shmmap == NULL)
	{
		SC_LOGI("shmmap fist access, so create it .");
		s_shmmap = (cmap*)malloc(sizeof(cmap));
		cmap_init(s_shmmap);
		s_shmmap_lock = cmtx_create();
		pthread_rwlock_init(&s_shm_rwlock, NULL);
	}

	cmtx_enter(s_shmmap_lock);
	void* memory = NULL;
	void* node = cmap_pkey_find(s_shmmap, name);
	if(node == NULL)
	{
		memory = (void*)malloc(size);
		memset(memory, 0, size);
		shmmap_node* n = (shmmap_node*)malloc(sizeof(shmmap_node));
		n->addr = memory;
		n->size = size;
		n->ref_count = 1;
		/*printf("if node - ref_count: %d\n", n->ref_count);*/
		snprintf(n->name, 64, "%s", name);
		int ret = cmap_pkey_insert(s_shmmap, name, (void*)n);
		if(ret != 0)
		{
			free(n);
			free(memory);
			memory = NULL;
			SC_LOGE("cmap_pkey_insert %s error", name);
		}
		SC_LOGI("[%s] node is null, so create, ref:[%d].", name, n->ref_count);
	}
	else
	{
		shmmap_node* n = (shmmap_node*)node;
		memory = n->addr;
		n->ref_count++;
		SC_LOGI("[%s] node is exit so add, ref:[%d].", name, n->ref_count);
		/*printf("else node - ref_count: %d\n", n->ref_count);*/
	}
	cmtx_leave(s_shmmap_lock);
	return memory;
}

int shm_stream_is_already_create(char* id, char* name, int max_users){
	if((id == NULL) || (name == NULL)){
		SC_LOGE("id or name is null.");
		return 0;
	}

	cmtx_enter(s_shmmap_lock);
	shmmap_node* node = (shmmap_node*)cmap_pkey_find(s_shmmap, name);
	if(node == NULL){
		cmtx_leave(s_shmmap_lock);
		return 0;
	}

	shm_user_t* users = (shm_user_t*)node->addr;
	if(users == NULL){
		cmtx_leave(s_shmmap_lock);
		SC_LOGE("[%s-%s] stream manager logic is error, map is found bug users is null.",
			id, name);
		return 0;
	}
	for (int i = 0; i < max_users; i++){
		if (strncmp(users[i].id, id, 32) == 0){
			cmtx_leave(s_shmmap_lock);
			SC_LOGW("[%s-%s] found is already created.", id, name);
			return 1;
		}
	}
	cmtx_leave(s_shmmap_lock);
	return 0;
}
/*
	为了避免同一个id未能正确调用destory而造成ref_count重复累计
*/
int  shm_stream_malloc_fix(shm_stream_t* handle, char* id, const char* name, int users, void* addr)
{
	if(handle == NULL) return -1;

	int i;
	shm_user_t* user = (shm_user_t*)addr;

	cmtx_enter(s_shmmap_lock);
	for (i=0; i<users; i++)
	{
		if (strncmp(user[i].id, id, 32) == 0)
		{
			void* node = cmap_pkey_find(s_shmmap, name);
			shmmap_node* n = (shmmap_node*)node;
			n->ref_count--;
		}
	}
	cmtx_leave(s_shmmap_lock);
	return 0;
}

void shm_stream_unmalloc(shm_stream_t* handle)
{
	if(handle == NULL) return;

	if(s_shmmap == NULL)
		return;

	cmtx_enter(s_shmmap_lock);
	void* node = cmap_pkey_find(s_shmmap, handle->name);

	if(node == NULL){
		cmtx_leave(s_shmmap_lock);
		return;
	}

	shmmap_node* n = (shmmap_node*)node;
	n->ref_count--;
	if(n->ref_count == 0)
	{
		SC_LOGI("map key:%s ref_count:%d, so destroy shm", handle->name, n->ref_count);
		free(n->addr);//free(handle->user_array);
		free(n);
		cmap_pkey_erase(s_shmmap, handle->name);
	}
	else
	{
		SC_LOGI("map key:%s current ref_count:%d", handle->name, n->ref_count);
	}
	cmtx_leave(s_shmmap_lock);
}

void* shm_stream_mmap(shm_stream_t* handle, const char* name, unsigned int size)
{
	if(handle == NULL) return NULL;

	int fd, shmid;
	void *memory;
	struct shmid_ds buf;

	char filename[32];
	snprintf(filename, 32, ".%s", name);
	if((fd = open(filename, O_RDWR|O_CREAT|O_EXCL, 0777)) > 0)
	{
		close(fd);
	}
	else
	{
		SC_LOGW("open name:%s error errno:%d %s", filename, errno, strerror(errno));
	}

	shmid = shmget(ftok(filename, 'g'), size, IPC_CREAT|0666);
	if(shmid == -1)
	{
		SC_LOGE("shmget errno:%d %s", errno, strerror(errno));
		return NULL;
	}

	memory = shmat(shmid, NULL, 0);
	if (memory == (void *)-1)
    {
        SC_LOGE("shmat failed errno:%d %s", errno, strerror(errno));
        return NULL;
    }

	shmctl(shmid, IPC_STAT, &buf);
	if (buf.shm_nattch == 1)
	{
		SC_LOGI("shm_nattch:%d", buf.shm_nattch);
		memset(memory, 0, size);
	}
	else
	{
		SC_LOGI("shm_nattch:%d", buf.shm_nattch);
	}

	return memory;
}

void shm_stream_unmap(shm_stream_t* handle)
{
	if(handle == NULL) return;

	shmdt(handle->user_array);
}

