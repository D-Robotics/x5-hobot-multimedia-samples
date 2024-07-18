#ifndef BPU_WRAP_H_
#define BPU_WRAP_H_

#include "utils/mqueue.h"
#include "utils/mthread.h"

#include "dnn/hb_dnn.h"

typedef int (*bpu_post_process_callback)(char* result, void *userdata);

// 模型推理函数的原型
typedef void *(*inference_function)(void *);

// 模型后处理函数的原型
typedef void *(*post_processing_function)(void *);

// 描述模型的结构体
typedef struct {
	char model_name[32]; // 模型名称
	char model_path[256]; // 模型文件路径
	inference_function inference_func; // 模型推理函数指针
	post_processing_function post_proc_func; // 模型后处理函数指针
} bpu_model_descriptor;

/* info :
 * y,uv             2 plane
 * raw              1 plane
 * raw, raw         2 plane(dol2)
 * raw, raw, raw    3 plane(dol3)
 **/
typedef struct bpu_buffer_info_s {
	int32_t plane_cnt;
	int32_t width;
	int32_t height;
	int32_t w_stride;
	uint8_t *addr[16];
	uint64_t paddr[16];
	struct timeval tv; // 送入数据对应的时间戳，在视频和算法结果同步时需要使用
} bpu_buffer_info_t;

// 这个结构体中存储的数据用来后处理时进行坐标还原
typedef struct {
	int32_t m_model_w;		// 模型输入宽
	int32_t m_model_h;		// 模型输入高
	int32_t m_ori_width;	// 用户看到的原始图像宽，比如web上显示的推流视频
	int32_t m_ori_height;	// 用户看到的原始图像高，比如web上显示的推流视频
} bpu_image_info_t;

typedef struct {
	hbDNNTensor m_dnn_tensor;
	struct timeval tv; // 送入数据对应的时间戳，在视频和算法结果同步时需要使用
} bpu_tensor_info_t;

#define BPU_INPUT_BUFFER_NUM 5

typedef struct {
	int32_t				m_vpp_id; // vedio pipeline id
	char				m_model_name[32];
	hbPackedDNNHandle_t	m_packed_dnn_handle;
	hbDNNHandle_t		m_dnn_handle;
	bpu_image_info_t	m_image_info;
	tsThread 			m_run_model_thread; // 运算模型的线程
	bpu_tensor_info_t	m_input_tensors[BPU_INPUT_BUFFER_NUM]; // 给bpu输入tensor预分配内存，避免每一帧数据都进行内存的申请和释放
	int32_t				m_cur_input_tensor; // 当前使用的 bpu input 内存序号
	tsQueue				m_input_queue; // 用于算法预测的yuv数据
	tsThread 			m_post_process_thread; // 算法后处理线程
	tsQueue				m_output_queue; // 算法输出结果队列，yolo5的后处理时间太长了，用线程分开处理
	bpu_post_process_callback	callback; // 算法结果处理后的回调，目前直接通过websocket发给web
	void				*m_userdata; // 回调函数中使用到的数据
} bpu_handle_t;

int32_t bpu_wrap_get_model_list(char *model_list);

int32_t bpu_wrap_init(bpu_handle_t *bpu_handle, char *model_file_name, char *model_name);
int bpu_wrap_deinit(bpu_handle_t *handle);

// 对bpu_wrap_init再次封装，主要是根据alog_id使用不同的模型文件
int32_t bpu_wrap_model_init(bpu_handle_t *bpu_handle, char *model_name);
void bpu_wrap_set_ori_hw(bpu_handle_t *handle, int32_t width, int32_t height);
int32_t bpu_wrap_get_model_hw(char *model_name, int32_t *width, int32_t *height);

int32_t bpu_wrap_start(bpu_handle_t *handle);
int32_t bpu_wrap_stop(bpu_handle_t *handle);

int32_t bpu_wrap_send_frame(bpu_handle_t *handle, bpu_buffer_info_t *input_buffer);

void bpu_wrap_callback_register(bpu_handle_t* handle, bpu_post_process_callback callback, void *userdata);
void bpu_wrap_callback_unregister(bpu_handle_t* handle);

int32_t bpu_wrap_general_result_handle(char *result, void *userdata);

void print_bpu_buffer_info(const bpu_buffer_info_t *buffer_info);

#endif // BPU_WRAP_H_