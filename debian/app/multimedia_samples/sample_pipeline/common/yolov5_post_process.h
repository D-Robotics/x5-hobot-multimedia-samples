// Copyright (c) 2024 D-Robotics.All Rights Reserved.
//
// The material in this file is confidential and contains trade secrets
// of D-Robotics Inc. This is proprietary information owned by
// D-Robotics Inc. No part of this work may be disclosed,
// reproduced, copied, transmitted, or used in any way for any purpose,
// without the express written permission of D-Robotics Inc.

#ifndef _POST_PROCESS_YOLOV5_POST_PROCESS_H_
#define _POST_PROCESS_YOLOV5_POST_PROCESS_H_

#include "dnn/hb_dnn.h"

#ifdef __cplusplus
	extern "C"{
#endif

typedef struct {
	int height;
	int width;
	int ori_height;
	int ori_width;
	float score_threshold; // 0.35
	float nms_threshold; // 0.65
	int nms_top_k; // 500
	int is_pad_resize;
	struct timeval tv; // 送入数据对应的时间戳，在视频和算法结果同步时需要使用
	hbDNNTensor *output_tensor;
} Yolov5PostProcessInfo_t;

	/**
	 * Post process
	 * @param[in] tensor: Model output tensors
	 * @param[in] image_tensor: Input image tensor
	 * @param[out] perception: Perception output data
	 * @return 0 if success
	 */
	char* Yolov5PostProcess(Yolov5PostProcessInfo_t *post_info);

#ifdef __cplusplus
}
#endif

#endif  // _POST_PROCESS_YOLOV5_POST_PROCESS_H_
