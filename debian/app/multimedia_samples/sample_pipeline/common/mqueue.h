#ifndef MQUEUE_H_
#define MQUEUE_H_

#include <stdint.h>

typedef enum
{
    E_QUEUE_OK,
    E_QUEUE_ERROR_FAILED,
    E_QUEUE_ERROR_TIMEOUT,
    E_QUEUE_ERROR_NO_MEM,
    E_QUEUE_ERROR_FULL,
	E_QUEUE_ERROR_REPEAT,
} teQueueStatus;

typedef struct
{
    void **apvBuffer;
    uint32_t u32Length;
    uint32_t u32Front;
    uint32_t u32Rear;

    pthread_mutex_t mutex;
    pthread_cond_t cond_space_available;
    pthread_cond_t cond_data_available;
} tsQueue;

typedef int(*queue_process_func_t)(void *data, void *handle);
typedef int(*queue_process_func_width_user_t)(void *data, void *handle, int user_flag);

teQueueStatus mQueueCreate(tsQueue *psQueue, uint32_t u32Length);
teQueueStatus mQueueDestroy(tsQueue *psQueue);
teQueueStatus mQueueEnqueue(tsQueue *psQueue, void *pvData);
teQueueStatus mQueueEnqueueEx(tsQueue *psQueue, void *pvData);
teQueueStatus mQueueEnqueueExWidhFunc(tsQueue *psQueue, void *pvData,
	queue_process_func_t process_func_cb, void *handle);

teQueueStatus mQueueDequeue(tsQueue *psQueue, void **ppvData);
teQueueStatus mQueueDequeueTimed(tsQueue *psQueue, uint32_t u32WaitTimeMil, void **ppvData);
teQueueStatus mQueueDequeueTimedWidthFunc(tsQueue *psQueue, uint32_t u32WaitTimeMil, void **ppvData,
	queue_process_func_t process_func_cb, void *handle);
teQueueStatus mQueueDequeueTimedWidthUserFunc(tsQueue *psQueue, uint32_t u32WaitTimeMil,
	void **ppvData, queue_process_func_width_user_t process_func_cb, void *handle, int user_flag);
int mQueueIsFull(tsQueue *psQueue);
int mQueueIsEmpty(tsQueue *psQueue);

#endif // MQUEUE_H_