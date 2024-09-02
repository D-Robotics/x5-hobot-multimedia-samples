#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include "video_queue.h"

#define DEBUG
#undef DEBUG			/* comment to show debug info */

#ifdef DEBUG
#define print printf
#else
#define print(...)
#endif

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * create and return a new queue, with size nodes in total
 **/
queue *queue_create(int size)
{
	queue *new_queue = malloc(sizeof(queue));
	if (!new_queue) {
		fprintf(stderr, "malloc failed creating the queue\n");
		return NULL;
	}

	new_queue->first = NULL;
	new_queue->last = NULL;
	new_queue->total = size;
	new_queue->num = 0;

	print("generated the queue @ %p\n", new_queue);

	return new_queue;
}

/**
 * destroy the queue
 **/
void queue_destroy(queue * que)
{
	print("Entered to queue_destroy");

	if (que == NULL)
		return;

	print("que is not null, que = %p\n", que);

	pthread_mutex_lock(&mutex);
	if (que->first == NULL) {
		print("que->first == NULL ...\n");
		free(que);
		pthread_mutex_unlock(&mutex);
		return;
	}

	/* destroy queue */
	node *_node = que->first;

	print("Destroy# first@ %p, last@ %p, num@ %u, total@ %u\n",
	      que->first, que->last, que->num, que->total);

	while (_node != NULL) {
		// freeing the data coz it's on the heap and no one to free it
		// except for this one
		print("freeing: %p, data: %p, next: %p\n",
		      _node, _node->data, _node->next);
		free(_node->data);
		node *tmp = _node->next;
		free(_node);
		_node = tmp;
	}

	free(que);
	que = NULL;

	pthread_mutex_unlock(&mutex);
}

/**
 * que is a queue pointer
 * total size bytes data will be copied into the queue
 **/
int enqueue(queue * que, void *data, uint32_t size)
{
	void *tmp;
	node *new_node;

	print("Enqueue# in\n");
	pthread_mutex_lock(&mutex);
	if (que->num >= que->total) {
		print("Enqueue# que is full. num(%u), total(%u)\n",
		      que->num, que->total);
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	new_node = malloc(sizeof(node));
	if (new_node == NULL) {
		pthread_mutex_unlock(&mutex);
		fprintf(stderr, "malloc failed to creating a node\n");
		return -1;
	}

	tmp = malloc(size);
	if (!tmp) {
		fprintf(stderr, "malloc failed for data copy\n");
		free(new_node);
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	/* copy data to tmp and add tail to the queue */
	memcpy(tmp, data, size);
	new_node->data = tmp;
	new_node->size = size;
	new_node->next = NULL;

	if (que->first == NULL) {
		// new queue
		que->first = new_node;
		que->last = new_node;
	} else {
		que->last->next = new_node;
		que->last = new_node;
	}
	que->num++;

	print("Enqueue# _node@ %p, first@ %p, last@ %p, num@ %u, total@ %u\n",
	      new_node, que->first, que->last, que->num, que->total);
	pthread_mutex_unlock(&mutex);
	print("Enqueue exit\n");

	return 0;
}

/**
 * que is a queue pointer
 * return the dequeue's buffer, size indicated the dequeue buffer length
 * if return NULL or size = 0, means no more buffer in queue
 * !! return's data should be used&freed by user
 **/
void *dequeue(queue * que, uint32_t * size)
{
	print("Dequeue# in\n");
	if (que == NULL) {
		print("que is null exsting...\n");
		return NULL;
	}

	pthread_mutex_lock(&mutex);
	if (que->first == NULL) {
		pthread_mutex_unlock(&mutex);
		print("Dequeue# que is empty.\n");
		return NULL;
	}

	void *data;
	node *_node = que->first;
	if (que->first == que->last) {
		que->first = NULL;
		que->last = NULL;
	} else {
		que->first = _node->next;
	}
	que->num--;

	data = _node->data;
	*size = _node->size;

	print("Free _node@ %p, first@ %p, last@ %p, num@ %u, total@ %u\n",
	      _node, que->first, que->last, que->num, que->total);
	free(_node);
	pthread_mutex_unlock(&mutex);
	print("Exiting deque\n");

	return data;
}

/**
 * que is a queue pointer
 * flush queue
 **/
void queue_flush(queue * que)
{
	print("Flush queue\n");
	if (que == NULL) {
		print("que is null exsting...\n");
		return;
	}

	pthread_mutex_lock(&mutex);
	if (que->first == NULL) {
		pthread_mutex_unlock(&mutex);
		print("queue is alread empty, no need do flush.\n");
		return;
	}

	/* flush queue node */
	node *_node = que->first;

	print("Flush# first@ %p, last@ %p, num@ %u, total@ %u\n",
	      que->first, que->last, que->num, que->total);

	while (_node != NULL) {
		// freeing the data coz it's on the heap and no one to free it
		// except for this one
		print("freeing: %p, data: %p, next: %p\n",
		      _node, _node->data, _node->next);
		free(_node->data);
		node *tmp = _node->next;
		free(_node);
		_node = tmp;
	}

	/* reset queue to empty */
	que->first = NULL;
	que->last = NULL;
	que->num = 0;

	pthread_mutex_unlock(&mutex);

	return;
}

/**
 * que is a queue pointer
 * queue is empty on below case:
 * que->first == que->last == NULL
 **/
int queue_is_empty(queue * que)
{
	int is_empty = 0;

	if (que == NULL) {
		print("que is null exsting...\n");
		return 0;
	}

	pthread_mutex_lock(&mutex);
	if (que->last == NULL) {
		print("queue is empty\n");
		is_empty = 1;
	}
	pthread_mutex_unlock(&mutex);

	return is_empty;
}

/**
 * que is a queue pointer
 * queue is full on below case:
 * que->num == que->total
 **/
int queue_is_full(queue * que)
{
	int is_full = 0;

	if (que == NULL) {
		print("que is null exsting...\n");
		return 0;
	}

	pthread_mutex_lock(&mutex);
	if (que->num == que->total)
		is_full = 1;
	pthread_mutex_unlock(&mutex);

	return is_full;
}
