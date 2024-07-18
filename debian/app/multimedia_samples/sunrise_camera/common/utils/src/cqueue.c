#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cqueue.h"

#ifdef __cplusplus
extern "C"{
#endif

void cqueue_init(cqueue *p)
{
	p->front = (cqueuenode *)malloc(sizeof(cqueuenode));
	p->front->data = NULL;
	p->rear = p->front;
	p->size = 0;
	(p->front)->next = 0;

	p->lock = cmtx_create();
}

int cqueue_destory(cqueue *q)
{
	cmtx_enter(q->lock);
	while (q->front)
	{
		q->rear = q->front->next;
		if(q->front->data)
			free(q->front->data);
		free(q->front);
		q->front = q->rear;
	}
	q->size = 0;
	cmtx_leave(q->lock);
	cmtx_delete(q->lock);
	return 0;
}

void cqueue_clear(cqueue *q)
{
	cmtx_enter(q->lock);
	
	while (cqueue_is_empty(q) != 1)
	{
		q->rear = q->front->next;
		if(q->front->data)
			free(q->front->data);
		free(q->front);
		q->front = q->rear;
	}
	if(q->front->data)
		free(q->front->data);
		
	q->front->data = NULL;
	q->rear = q->front;
	q->size = 0;
	(q->front)->next = 0;
	
	cmtx_leave(q->lock);
}

int cqueue_is_empty(cqueue *q)
{
	cmtx_enter(q->lock);
	int v;
	if (q->front == q->rear) 
		v = 1;
	else              
		v = 0;
	cmtx_leave(q->lock);
	return v;
}

void* cqueue_get(cqueue *q, int index)
{
	int i = 0;
	cmtx_enter(q->lock);
	cqueuenode *p = q->front;
	void* v = 0;
	while(q->front != q->rear)
	{
		p = p->next;
		v = p->data;
		if(i == index)
			break;
		i++;
	}
	cmtx_leave(q->lock);
	return v;
}

void* cqueue_gethead(cqueue *q)
{
	cmtx_enter(q->lock);
	void* v;
	if (q->front == q->rear)
		v = 0;
	else
		v = (q->front)->next->data;
	cmtx_leave(q->lock);
	return v;
}

int cqueue_enqueue(cqueue *q, void* e)
{
	cmtx_enter(q->lock);
	q->rear->next = (cqueuenode *)malloc(sizeof(cqueuenode));
	q->rear = q->rear->next;
	if (!q->rear){
		cmtx_leave(q->lock);
		return -1;
	}
	else
	{
		q->rear->data = e;
		q->rear->next = 0;
		q->size += 1;
		cmtx_leave(q->lock);
		
		return 0;
	}
}

void* cqueue_dequeue(cqueue *q)
{
	cmtx_enter(q->lock);
	cqueuenode *p;
	void* e;
	if (q->front == q->rear)
	{
		e = 0;
	}
	else 
	{
		p = (q->front)->next;
		(q->front)->next = p->next;
		e = p->data;
		if (q->rear == p)
			q->rear = q->front;
		free(p);
		q->size -= 1;
	}
	cmtx_leave(q->lock);
	return(e);
}

int cqueue_size(cqueue *q)
{
	cmtx_enter(q->lock);
	int size = q->size;
	cmtx_leave(q->lock);
	return size;
}


array_queue* array_queue_create(int max, int node_size)
{
	array_queue* q = (array_queue*)malloc(sizeof(array_queue));

	q->max = max + 1;	//有一个预留
	q->node_size = node_size;
	q->base = (char*)malloc(q->max * node_size);
	q->front = 0;
	q->tail = 0;
	q->lock = cmtx_create();
	
	return q;
}

void array_queue_clear(array_queue *q)
{
	if(q == NULL) return;
	
	cmtx_enter(q->lock);
	q->tail = q->front;
	cmtx_leave(q->lock);
}
void array_queue_destory(array_queue *q)
{
	if(q == NULL) return;

	cmtx_delete(q->lock);
	free(q->base);
	free(q);
}
int array_queue_is_empty(array_queue *q)
{
	if(q == NULL) return 0;

	if(q->tail == q->front)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}
int array_queue_is_full(array_queue *q)
{
	if(q == NULL) return -1;

	if((q->front + 1) % q->max == q->tail)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

int array_queue_push(array_queue *q, void* e)
{
	if(q == NULL) return -1;

	cmtx_enter(q->lock);
	if((q->front + 1) % q->max == q->tail)
	{
//		printf("frame_infos overflow\n");
		q->tail = (q->tail + 1) % q->max;
	}
//	printf("q->front:%d q->max:%d\n", q->front, q->max);
	memset(q->base + q->node_size*q->front, 0, q->node_size);
	memcpy(q->base + q->node_size*q->front, e, q->node_size);
	
	q->front = (q->front + 1) % q->max;
	cmtx_leave(q->lock);

	return 0;
}

int array_queue_pop(array_queue *q, void* e)
{
	if(q == NULL) return -1;

	if(array_queue_is_empty(q) == 0)  return -1;
	
	cmtx_enter(q->lock);
	memcpy(e, q->base + q->node_size*q->tail, q->node_size);
	q->tail = (q->tail + 1) % q->max;
//	printf("q->tail:%d q->max:%d\n", q->tail, q->max);
	cmtx_leave(q->lock);

	return 0;
}
int array_queue_front(array_queue *q, void* e)
{
	if(q == NULL) return -1;

	if(array_queue_is_empty(q) == 0)  return -1;
	
	cmtx_enter(q->lock);
	memcpy(e, q->base + q->node_size*q->tail, q->node_size);
	cmtx_leave(q->lock);

	return 0;
}

int array_queue_post(array_queue *q)
{
	if(q == NULL) return -1;

	if(array_queue_is_empty(q) == 0)  return -1;
	
	cmtx_enter(q->lock);
	q->tail = (q->tail + 1) % q->max;
	cmtx_leave(q->lock);

	return 0;
}

int array_queue_get(array_queue *q, int i, void* e)
{
	if(q == NULL) return -1;
	
	if(array_queue_is_empty(q) == 0)  return -1;
	if((q->tail+i)%q->max == q->front)   return -1;
		
	cmtx_enter(q->lock);
	memcpy(e, q->base + q->node_size*((q->tail+i)%q->max), q->node_size);
	cmtx_leave(q->lock);

	return 0;
}

int array_queue_remaid(array_queue *q)
{
	if(q == NULL) return -1;

	return ((q->front+q->max)  - q->tail) % q->max;
}

#ifdef __cplusplus
}
#endif

