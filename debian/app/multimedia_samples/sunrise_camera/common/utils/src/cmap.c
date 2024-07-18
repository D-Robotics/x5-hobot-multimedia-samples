#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cmap.h"

#ifdef __cplusplus
extern "C"{
#endif


void cmap_init(cmap *p)
{
	p->front = (cmapnode *)malloc(sizeof(cmapnode));
	p->front->data = NULL;
	p->rear = p->front;
	p->size = 0;
	p->front->key.i_key = 0xffffffff;
	memset(p->front->key.p_key, 0, 64);
	(p->front)->next = 0;
	p->lock = cmtx_create();
}

void cmap_clear(cmap *q)
{
	cmtx_enter(q->lock);
	while (cmap_is_empty(q) != 1)
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
	q->front->key.i_key = 0xffffffff;
	memset(q->front->key.p_key, 0, 64);
	q->size = 0;
	cmtx_leave(q->lock);
}

int cmap_destory(cmap *q)
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

int cmap_is_empty(cmap *q)
{
	int v;
	if (q->front == q->rear) 
		v = 1;
	else              
		v = 0;
	return v;
}

int cmap_ikey_insert(cmap *q, int key, void* e)
{
	if(cmap_ikey_find(q, key) != 0) return -2;

	cmtx_enter(q->lock);
	q->rear->next = (cmapnode *)malloc(sizeof(cmapnode));
	q->rear = q->rear->next;
	if (!q->rear) 
	{
		cmtx_leave(q->lock);
		return -1;
	}
	q->rear->data = e;
	q->rear->key.i_key = key;
	q->rear->next = 0;
	q->size += 1;
	cmtx_leave(q->lock);

	return 0;
}

void* cmap_ikey_find(cmap *q, int key)
{
	void* v = 0;
	cmtx_enter(q->lock);
	cmapnode* p = q->front->next;
	while (p)
	{	
		if(p->key.i_key == key)
		{
			v = p->data;
			break;
		}
		p = p->next;
	}
	cmtx_leave(q->lock);

	return v;
}

int cmap_ikey_erase(cmap *q, int key)
{
	cmtx_enter(q->lock);
	cmapnode* t = q->front;
	cmapnode* p = q->front->next;
	while (p)
	{	
		if(p->key.i_key == key)
		{
			t->next = p->next;
			if(q->rear == p)
			{
				q->rear = t;
			}
			
			free(p);

			q->size -= 1;
			cmtx_leave(q->lock);
			return 0;
		}
		t = p;
		p = p->next;
	}
	cmtx_leave(q->lock);

	return -1;
}

int cmap_pkey_insert(cmap *q, const char* key, void* e)
{
	if(cmap_pkey_find(q, key) != 0) return -2;

	cmtx_enter(q->lock);
	q->rear->next = (cmapnode *)malloc(sizeof(cmapnode));
	q->rear = q->rear->next;
	if (!q->rear)
	{
		cmtx_leave(q->lock);
		return -1;
	}
	else
	{
		q->rear->data = e;
		strcpy(q->rear->key.p_key, key);
		q->rear->next = 0;
		q->size += 1;

		cmtx_leave(q->lock);
		
		return 0;
	}

}

//将释放节点数据内存
int cmap_pkey_erase(cmap *q, const char* key)
{
	cmtx_enter(q->lock);
	cmapnode* t = q->front;
	cmapnode* p = q->front->next;
	while (p)
	{	
		if(strcmp(p->key.p_key, key) == 0)
		{
			t->next = p->next;
			if(q->rear == p)
			{
				q->rear = t;
			}
			
			free(p);
			q->size -= 1;
			
			cmtx_leave(q->lock);
			return 0;
		}
		t = p;
		p = p->next;
	}
	cmtx_leave(q->lock);
	
	return -1;
}

void* cmap_pkey_find(cmap *q, const char* key)
{
	cmtx_enter(q->lock);
	void* v = 0;
	cmapnode* p = q->front->next;
	while (p)
	{	
		if(strcmp(p->key.p_key, key) == 0)
		{
			v = p->data;
			break;
		}
		p = p->next;
	}
	cmtx_leave(q->lock);
	
	return v;
}

cmapnode* cmap_index_get(cmap *q, int index)
{
	cmapnode* v = 0;
	int idx = 0;
	cmtx_enter(q->lock);
	cmapnode* p = q->front->next;
	while (p)
	{	
		if(idx == index)
		{
			v = p;
			break;
		}
		p = p->next;
		idx++;
	}
	cmtx_leave(q->lock);

	return v;
}

int cmap_size(cmap *q)
{
	return q->size;
}

#ifdef __cplusplus
}
#endif

