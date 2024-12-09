#ifndef H_SYNCHRONOUS_PRODUCT_AND_CONSUM_QUEUE__H
#define H_SYNCHRONOUS_PRODUCT_AND_CONSUM_QUEUE__H
#include <stdint.h>
#include <pthread.h>

#include "mqueue.h"
#define SYNC_QUEUE_MAX_USER 5
typedef struct data_item_s{
	int is_init_added; //初始化过程中会向 unused队列中添加数据，但是不用归还内存
	int item_count;
	void *items;
	int ref_repay;
	int ref_obtain;
	int index;
	uint64_t inused_update_time_ms;
	uint64_t unused_update_time_ms;

	int inused_frame_index;
	int unused_frame_index;
	uint8_t user_recoder[SYNC_QUEUE_MAX_USER];
}data_item_t;

typedef int(*sync_queue_cb_func_t)(void *, void*);

typedef struct sync_queue_info_s{
	char *productor_name;
	char *consumer_name;
	int is_need_malloc_in_advance;
	int is_external_buffer;
	int queue_len;
	int data_item_size;
	int data_item_count;

	void *item_data_init_param;
	sync_queue_cb_func_t item_data_init_func;

	void *item_data_deinit_param;
	sync_queue_cb_func_t item_data_deinit_func;
}sync_queue_info_t;

typedef struct sync_queue_s{
	int user_count; //inused queue

	tsQueue ununsed_queue;
	tsQueue inused_queue;
	int inused_queue_count;

	sync_queue_info_t sync_queue_info;
}sync_queue_t;

int sync_queue_add_user(sync_queue_t *sync_queue);
int sync_queue_create(sync_queue_t *sync_queue, sync_queue_info_t *sync_queue_info);

//for productor
int sync_queue_get_unused_object(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t **data_item);
int sync_queue_save_inused_object(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t *data_item);

//for consumer
int sync_queue_obtain_inused_object(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t **data_item);
int sync_queue_obtain_inused_object_width_user(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t **data_item,
	int user_flag);
int sync_queue_repay_unused_object(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t *data_item);

int sync_queue_destory(sync_queue_t *sync_queue);

int sync_queue_create_multi_user(sync_queue_t *sync_queue, sync_queue_info_t *sync_queue_info);
#endif
