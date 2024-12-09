#ifndef  __BPU_COMMON_H__
#define __BPU_COMMON_H__

typedef struct{
	int x;
	int y;
	int width;
	int height;
	const char *label;
} detect_object_t;


#define DETECT_OBJECT_COUNT 100
typedef struct{
	int valid_count;
	detect_object_t detect_objects[DETECT_OBJECT_COUNT];
} detect_object_array_t;

#endif // ! __BPU_COMMON_H__