#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "synchronous_queue.h"
int sync_queue_create(sync_queue_t *sync_queue, sync_queue_info_t *sync_queue_info){

	int ret = 0;
	teQueueStatus status = mQueueCreate(&sync_queue->ununsed_queue, sync_queue_info->queue_len + 1); //必须是加1的
	if(status != E_QUEUE_OK){
		printf("mqueue create failed %d.", status);
		return -1;
	}
	status = mQueueCreate(&sync_queue->inused_queue, sync_queue_info->queue_len + 1);
	if(status != E_QUEUE_OK){
		printf("mqueue create failed %d.", status);
		return -1;
	}
	for (size_t j = 0; j < sync_queue_info->queue_len; j++){
		void *items = (void *)malloc(sync_queue_info->data_item_size * sync_queue_info->data_item_count);
		if (items == NULL){
			printf("malloc failed: queue item.\n");
			return -1;
		}
		memset(items, 0, sync_queue_info->data_item_size);


		if(sync_queue_info->item_data_init_func){
			for(int k = 0; k < sync_queue_info->data_item_count; k++){
				void *item_data = (void *)((uint8_t *)items + sync_queue_info->data_item_size * k);
				ret = sync_queue_info->item_data_init_func(sync_queue_info->item_data_init_param, item_data);
				if(ret != 0){

					printf("item_data_init_func failed.\n");
					return -1;
				}
			}
		}
		data_item_t *item_data = (data_item_t *)malloc(sizeof(data_item_t));
		if (item_data == NULL){
			printf("malloc failed: item data struct.\n");
			return -1;
		}
		memset(item_data, 0, sizeof(data_item_t));
		item_data->is_init_added = 1;
		item_data->item_count = sync_queue_info->data_item_count;
		item_data->items = items;

		teQueueStatus status = mQueueEnqueue(&sync_queue->ununsed_queue, (void *)item_data);
		if (status != E_QUEUE_OK){
			printf("mqueue enqueue failed:%d\n", status);
			return -1;
		}
	}
	sync_queue->sync_queue_info = *sync_queue_info;
	return 0;
}

//for productor
int sync_queue_get_unused_object(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t **data_item){
	const int timeout_duration_ms = 1000;
	int run_count = 0;
	while(timeout_duration_ms * run_count < timeout_ms){
		run_count++;

		teQueueStatus status = E_QUEUE_OK;
		status = mQueueDequeueTimed(&sync_queue->ununsed_queue, 1000, (void **)data_item);
		if(status != E_QUEUE_OK){
			printf("[%s -> %s] sync_queue_get_unused_object failed:%d, wait time %d\n",
				sync_queue->sync_queue_info.productor_name,
				sync_queue->sync_queue_info.consumer_name ,
				status, timeout_duration_ms * run_count);
			continue;
		}
		break;
	}
	return 0;
}
int sync_queue_save_inused_object(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t *data_item){
	int ret = -1;
	teQueueStatus status = E_QUEUE_OK;
	const uint32_t check_duration_ms = 500;

	uint32_t timeout_ms_consume = 0;
	data_item->is_init_added = 0;
	while(timeout_ms_consume < timeout_ms){

		status = mQueueEnqueueEx(&sync_queue->inused_queue, data_item);
		if (status != E_QUEUE_OK){
			printf("[%s -> %s] sync_queue_save_inused_object failed:%d, already wait %dms\n",
			sync_queue->sync_queue_info.productor_name,
			sync_queue->sync_queue_info.consumer_name ,
			status, timeout_ms_consume);
			usleep(check_duration_ms * 1000);
			timeout_ms_consume += check_duration_ms;
			continue;
		}
		ret = 0;
		break;
	}

	return ret;
}

//for consumer
int sync_queue_obtain_inused_object(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t **data_item){
	const int timeout_duration_ms = 1000;
	int run_count = 0;
	while(timeout_duration_ms * run_count < timeout_ms){
		run_count++;

		teQueueStatus status = E_QUEUE_OK;
		status = mQueueDequeueTimed(&sync_queue->inused_queue, 1000, (void **)data_item);
		if(status != E_QUEUE_OK){
			printf("[%s -> %s] sync_queue_get_unused_object failed:%d, wait time %d\n",
				sync_queue->sync_queue_info.productor_name,
				sync_queue->sync_queue_info.consumer_name ,
				status, timeout_duration_ms * run_count);
			continue;
		}
		break;
	}

	return 0;
}

int sync_queue_repay_unused_object(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t *data_item){
	int ret = -1;
	teQueueStatus status = E_QUEUE_OK;
	const uint32_t check_duration_ms = 500;

	uint32_t timeout_ms_consume = 0;
	data_item->is_init_added = 0;
	while(timeout_ms_consume < timeout_ms){

		status = mQueueEnqueueEx(&sync_queue->ununsed_queue, data_item);
		if (status != E_QUEUE_OK){
			printf("[%s -> %s] sync_queue_repay_unused_object failed:%d, already wait %dms\n",
			sync_queue->sync_queue_info.productor_name,
			sync_queue->sync_queue_info.consumer_name ,

			status, timeout_ms_consume);
			usleep(check_duration_ms * 1000);
			timeout_ms_consume += check_duration_ms;
			continue;
		}
		ret = 0;
		break;
	}

	return ret;
}

int sync_queue_destory(sync_queue_t *sync_queue)
{
	int inused_count = 0;
	int unused_count = 0;
	teQueueStatus status = E_QUEUE_OK;
	while(!mQueueIsEmpty(&sync_queue->inused_queue)){
		data_item_t *data_item = NULL;
		status = mQueueDequeueTimed(&sync_queue->inused_queue, 0, (void **)&data_item);
		if(status != E_QUEUE_OK){
			printf("mqueue clear failed %d.", status);
			break;
		}
		free(data_item->items);
		free(data_item);
		inused_count++;
	}
	status = mQueueDestroy(&sync_queue->inused_queue);
	if(status != E_QUEUE_OK){
		printf("mqueue destroy failed %d.", status);
	}

	while(!mQueueIsEmpty(&sync_queue->ununsed_queue)){
		data_item_t *data_item = NULL;
		status = mQueueDequeueTimed(&sync_queue->ununsed_queue, 0, (void **)&data_item);
		if(status != E_QUEUE_OK){
			printf("mqueue clear failed %d.", status);
			break;
		}
		free(data_item->items);
		free(data_item);
		unused_count++;
	}
	status = mQueueDestroy(&sync_queue->ununsed_queue);
	if(status != E_QUEUE_OK){
		printf("mqueue destroy failed %d.", status);
	}

return 0;
}