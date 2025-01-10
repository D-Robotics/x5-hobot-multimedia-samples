// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"
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
		item_data->index = j;

		teQueueStatus status = mQueueEnqueue(&sync_queue->ununsed_queue, (void *)item_data);
		if (status != E_QUEUE_OK){
			printf("mqueue enqueue failed:%d\n", status);
			return -1;
		}
	}
	sync_queue->sync_queue_info = *sync_queue_info;
	sync_queue->user_count = 1;
	return 0;
}
int sync_queue_add_user(sync_queue_t *sync_queue){
	sync_queue->user_count++;
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
	data_item->ref_repay = sync_queue->user_count;
	data_item->ref_obtain = sync_queue->user_count;
	while(timeout_ms_consume < timeout_ms){
		data_item->update_time_ms = get_timestamp_ms();

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
int dequeue_process_func(void *data, void *handle){

	sync_queue_t *sync_queue = (sync_queue_t*)handle;
	data_item_t* data_item = (data_item_t*)data;
	if(data_item->ref_obtain <= 0){
		printf("dequeue_process_func error,  found data ref count is %d, so exit process.\n",
			data_item->ref_obtain);
		exit(-1);
	}

	if(sync_queue->user_count == 2){
		// printf("de: %d\n", data_item->ref);
	}

	data_item->ref_obtain--;
	if(data_item->ref_obtain == 0){
		return 1;
	}
	return 0;
}

int enqueue_process_func(void *data, void *handle){
	sync_queue_t *sync_queue = (sync_queue_t*)handle;
	data_item_t* data_item = (data_item_t*)data;

	if(sync_queue->user_count == 2){
		// printf("en: %d\n", data_item->ref);
	}
	data_item->ref_repay--;
	if(data_item->ref_repay == 0){
		return 1;
	}

#if 0
	if(data_item->ref > 0){
		return 0; //使用者还没有用完
	}else if(data_item->ref == 0){
		data_item->ref = -1;
		return 1; //本次需要添加到队列
	}else if(data_item->ref == -1){
		return 0; //已经添加到队列
	}else{
		//被使用次数超过了使用者
		printf("enqueue_process_func error,  found data ref count is %d, so exit process.\n",
			data_item->ref_repay);
		exit(-1);
	}
#endif
	return 0;
}

//for consumer
int sync_queue_obtain_inused_object(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t **data_item){
	int ret = -1;
	const int timeout_duration_ms = 1000;
	int run_count = 0;
	while(timeout_duration_ms * run_count < timeout_ms){
		run_count++;

		teQueueStatus status = E_QUEUE_OK;
		status = mQueueDequeueTimedWidthFunc(&sync_queue->inused_queue, timeout_duration_ms, (void **)data_item,
			dequeue_process_func, sync_queue);
		if(status != E_QUEUE_OK){
			printf("[%s -> %s] sync_queue_obtain_inused_object failed:%d, wait time %d\n",
				sync_queue->sync_queue_info.productor_name,
				sync_queue->sync_queue_info.consumer_name ,
				status, timeout_duration_ms * run_count);
			continue;
		}
		ret = 0;
		break;
	}

	return ret;
}

int sync_queue_repay_unused_object(sync_queue_t* sync_queue, uint32_t timeout_ms, data_item_t *data_item){
	int ret = -1;
	teQueueStatus status = E_QUEUE_OK;
	const uint32_t check_duration_ms = 500;

	uint32_t timeout_ms_consume = 0;
	data_item->is_init_added = 0;
	while(timeout_ms_consume < timeout_ms){
		data_item->update_time_ms = get_timestamp_ms();
		status = mQueueEnqueueExWidhFunc(&sync_queue->ununsed_queue, data_item, enqueue_process_func, sync_queue);
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