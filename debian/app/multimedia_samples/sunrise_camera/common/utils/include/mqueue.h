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

teQueueStatus mQueueCreate(tsQueue *psQueue, uint32_t u32Length);
teQueueStatus mQueueDestroy(tsQueue *psQueue);
teQueueStatus mQueueEnqueue(tsQueue *psQueue, void *pvData);
teQueueStatus mQueueEnqueueEx(tsQueue *psQueue, void *pvData);
teQueueStatus mQueueDequeue(tsQueue *psQueue, void **ppvData);
teQueueStatus mQueueDequeueTimed(tsQueue *psQueue, uint32_t u32WaitTimeMil, void **ppvData);
int mQueueIsFull(tsQueue *psQueue);
int mQueueIsEmpty(tsQueue *psQueue);

#endif // MQUEUE_H_