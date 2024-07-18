// Copyright (c) 2024 D-Robotics.All Rights Reserved.
//
// The material in this file is confidential and contains trade secrets
// of D-Robotics Inc. This is proprietary information owned by
// D-Robotics Inc. No part of this work may be disclosed,
// reproduced, copied, transmitted, or used in any way for any purpose,
// without the express written permission of D-Robotics Inc.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>

#include "utils/utils_log.h"

#include "fcos_post_process.h"

/**
 * Config definition for Fcos
 */
struct PTQFcosConfig
{
	std::vector<int> strides;
	int class_num;
	std::vector<std::string> class_names;
	std::string det_name_list;

	std::string Str()
	{
		std::stringstream ss;
		ss << "strides: ";
		for (const auto &stride : strides)
		{
			ss << stride << " ";
		}
		ss << "; class_num: " << class_num;
		return ss.str();
	}
};

PTQFcosConfig default_ptq_fcos_config = {
	{{8, 16, 32, 64, 128}},
	80,
	{"person", "bicycle", "car",
	 "motorcycle", "airplane", "bus",
	 "train", "truck", "boat",
	 "traffic light", "fire hydrant", "stop sign",
	 "parking meter", "bench", "bird",
	 "cat", "dog", "horse",
	 "sheep", "cow", "elephant",
	 "bear", "zebra", "giraffe",
	 "backpack", "umbrella", "handbag",
	 "tie", "suitcase", "frisbee",
	 "skis", "snowboard", "sports ball",
	 "kite", "baseball bat", "baseball glove",
	 "skateboard", "surfboard", "tennis racket",
	 "bottle", "wine glass", "cup",
	 "fork", "knife", "spoon",
	 "bowl", "banana", "apple",
	 "sandwich", "orange", "broccoli",
	 "carrot", "hot dog", "pizza",
	 "donut", "cake", "chair",
	 "couch", "potted plant", "bed",
	 "dining table", "toilet", "tv",
	 "laptop", "mouse", "remote",
	 "keyboard", "cell phone", "microwave",
	 "oven", "toaster", "sink",
	 "refrigerator", "book", "clock",
	 "vase", "scissors", "teddy bear",
	 "hair drier", "toothbrush"},
	""};

struct ScoreId
{
	float score;
	int id;
};

/**
 * Finds the smallest element in the range [first, last).
 * @tparam[in] ForwardIterator
 * @para[in] first: first iterator
 * @param[in] last: last iterator
 * @return Iterator to the smallest element in the range [first, last)
 */
template <class ForwardIterator>
inline size_t argmin(ForwardIterator first, ForwardIterator last)
{
	return std::distance(first, std::min_element(first, last));
}

/**
 * Finds the greatest element in the range [first, last)
 * @tparam[in] ForwardIterator: iterator type
 * @param[in] first: fist iterator
 * @param[in] last: last iterator
 * @return Iterator to the greatest element in the range [first, last)
 */
template <class ForwardIterator>
inline size_t argmax(ForwardIterator first, ForwardIterator last)
{
	return std::distance(first, std::max_element(first, last));
}

/**
 * Bounding box definition
 */
typedef struct Bbox
{
	float xmin{0.0};
	float ymin{0.0};
	float xmax{0.0};
	float ymax{0.0};

	Bbox() {}

	Bbox(float xmin, float ymin, float xmax, float ymax)
		: xmin(xmin), ymin(ymin), xmax(xmax), ymax(ymax) {}

	friend std::ostream &operator<<(std::ostream &os, const Bbox &bbox)
	{
		const auto precision = os.precision();
		const auto flags = os.flags();
		os << "[" << std::fixed << std::setprecision(6) << bbox.xmin << ","
		   << bbox.ymin << "," << bbox.xmax << "," << bbox.ymax << "]";
		os.flags(flags);
		os.precision(precision);
		return os;
	}

	~Bbox() {}
} Bbox;

typedef struct Detection
{
	int id{0};
	float score{0.0};
	Bbox bbox;
	const char *class_name{nullptr};
	Detection() {}

	Detection(int id, float score, Bbox bbox)
		: id(id), score(score), bbox(bbox) {}

	Detection(int id, float score, Bbox bbox, const char *class_name)
		: id(id), score(score), bbox(bbox), class_name(class_name) {}

	friend bool operator>(const Detection &lhs, const Detection &rhs)
	{
		return (lhs.score > rhs.score);
	}

	friend std::ostream &operator<<(std::ostream &os, const Detection &det)
	{
		const auto precision = os.precision();
		const auto flags = os.flags();
		os << "{"
		   << R"("bbox")"
		   << ":" << det.bbox << ","
		   << R"("prob")"
		   << ":" << std::fixed << std::setprecision(6) << det.score << ","
		   << R"("label")"
		   << ":" << det.id << ","
		   << R"("class_name")"
		   << ":\"" << det.class_name << "\"}";
		os.flags(flags);
		os.precision(precision);
		return os;
	}

	~Detection() {}
} Detection;

static int get_tensor_hwc_index(hbDNNTensor *tensor,
								int *h_index,
								int *w_index,
								int *c_index)
{
	if (tensor->properties.tensorLayout == HB_DNN_LAYOUT_NHWC)
	{
		*h_index = 1;
		*w_index = 2;
		*c_index = 3;
	}
	else if (tensor->properties.tensorLayout == HB_DNN_LAYOUT_NCHW)
	{
		*c_index = 1;
		*h_index = 2;
		*w_index = 3;
	}
	else
	{
		return -1;
	}
	return 0;
}

static void fcos_nms(std::vector<Detection> &input,
					 float iou_threshold,
					 int top_k,
					 std::vector<Detection> &result,
					 bool suppress)
{
	// sort order by score desc
	std::stable_sort(input.begin(), input.end(), std::greater<Detection>());

	std::vector<bool> skip(input.size(), false);

	// pre-calculate boxes area
	std::vector<float> areas;
	areas.reserve(input.size());
	for (size_t i = 0; i < input.size(); i++)
	{
		float width = input[i].bbox.xmax - input[i].bbox.xmin;
		float height = input[i].bbox.ymax - input[i].bbox.ymin;
		areas.push_back(width * height);
	}

	int count = 0;
	for (size_t i = 0; count < top_k && i < skip.size(); i++)
	{
		if (skip[i])
		{
			continue;
		}
		skip[i] = true;
		++count;

		for (size_t j = i + 1; j < skip.size(); ++j)
		{
			if (skip[j])
			{
				continue;
			}
			if (suppress == false)
			{
				if (input[i].id != input[j].id)
				{
					continue;
				}
			}

			// intersection area
			float xx1 = std::max(input[i].bbox.xmin, input[j].bbox.xmin);
			float yy1 = std::max(input[i].bbox.ymin, input[j].bbox.ymin);
			float xx2 = std::min(input[i].bbox.xmax, input[j].bbox.xmax);
			float yy2 = std::min(input[i].bbox.ymax, input[j].bbox.ymax);

			if (xx2 > xx1 && yy2 > yy1)
			{
				float area_intersection = (xx2 - xx1) * (yy2 - yy1);
				float iou_ratio =
					area_intersection / (areas[j] + areas[i] - area_intersection);
				if (iou_ratio > iou_threshold)
				{
					skip[j] = true;
				}
			}
		}
		result.push_back(input[i]);
	}
}

static void GetBboxAndScoresNoneNHWC(
	hbDNNTensor *tensors,
	FcosPostProcessInfo_t *post_info,
	std::vector<Detection> &dets)
{
	int ori_h = post_info->ori_height;
	int ori_w = post_info->ori_width;
	int input_h = post_info->height;
	int input_w = post_info->width;
	float w_scale;
	float h_scale;
	// preprocess action is pad and resize
	if (post_info->is_pad_resize)
	{
		float scale = ori_h > ori_w ? ori_h : ori_w;
		w_scale = scale / input_w;
		h_scale = scale / input_h;
	}
	else
	{
		w_scale = static_cast<float>(ori_w) / input_w;
		h_scale = static_cast<float>(ori_h) / input_h;
	}

	// fcos stride is {8, 16, 32, 64, 128}
	for (int i = 0; i < 5; i++)
	{
		auto *cls_data = reinterpret_cast<float *>(tensors[i].sysMem[0].virAddr);
		auto *bbox_data =
			reinterpret_cast<float *>(tensors[i + 5].sysMem[0].virAddr);
		auto *ce_data =
			reinterpret_cast<float *>(tensors[i + 10].sysMem[0].virAddr);

		// 同一个尺度下，tensor[i],tensor[i+5],tensor[i+10]出来的hw都一致，64*64/32*32/...
		int *shape = tensors[i].properties.alignedShape.dimensionSize;
		int tensor_h = shape[1];
		int tensor_w = shape[2];
		int tensor_c = shape[3];

		for (int h = 0; h < tensor_h; h++)
		{
			int offset = h * tensor_w;
			for (int w = 0; w < tensor_w; w++)
			{
				// get score
				int ce_offset = offset + w;
				ce_data[ce_offset] = 1.0 / (1.0 + exp(-ce_data[ce_offset]));

				int cls_offset = ce_offset * tensor_c;
				ScoreId tmp_score = {cls_data[cls_offset], 0};
				for (int cls_c = 1; cls_c < tensor_c; cls_c++)
				{
					int cls_index = cls_offset + cls_c;
					if (cls_data[cls_index] > tmp_score.score)
					{
						tmp_score.id = cls_c;
						tmp_score.score = cls_data[cls_index];
					}
				}
				tmp_score.score = 1.0 / (1.0 + exp(-tmp_score.score));
				tmp_score.score = std::sqrt(tmp_score.score * ce_data[ce_offset]);
				if (tmp_score.score <= post_info->score_threshold)
					continue;

				// get detection box
				Detection detection;
				int index = 4 * (h * tensor_w + w);
				auto &strides = default_ptq_fcos_config.strides;

				detection.bbox.xmin =
					((w + 0.5) * strides[i] - bbox_data[index]) * w_scale;
				detection.bbox.ymin =
					((h + 0.5) * strides[i] - bbox_data[index + 1]) * h_scale;
				detection.bbox.xmax =
					((w + 0.5) * strides[i] + bbox_data[index + 2]) * w_scale;
				detection.bbox.ymax =
					((h + 0.5) * strides[i] + bbox_data[index + 3]) * h_scale;

				detection.score = tmp_score.score;
				detection.id = tmp_score.id;
				detection.class_name = default_ptq_fcos_config.class_names[detection.id].c_str();
				dets.push_back(detection);
			}
		}
	}
}

static void GetBboxAndScoresNoneNCHW(
	hbDNNTensor *tensors,
	FcosPostProcessInfo_t *post_info,
	std::vector<Detection> &dets)
{
	int ori_h = post_info->ori_height;
	int ori_w = post_info->ori_width;
	int input_h = post_info->height;
	int input_w = post_info->width;
	float w_scale;
	float h_scale;
	// preprocess action is pad and resize
	if (post_info->is_pad_resize)
	{
		float scale = ori_h > ori_w ? ori_h : ori_w;
		w_scale = scale / input_w;
		h_scale = scale / input_h;
	}
	else
	{
		w_scale = static_cast<float>(ori_w) / input_w;
		h_scale = static_cast<float>(ori_h) / input_h;
	}

	for (int i = 0; i < 5; i++)
	{
		auto *cls_data = reinterpret_cast<float *>(tensors[i].sysMem[0].virAddr);
		auto *bbox_data =
			reinterpret_cast<float *>(tensors[i + 5].sysMem[0].virAddr);
		auto *ce_data =
			reinterpret_cast<float *>(tensors[i + 10].sysMem[0].virAddr);

		// 同一个尺度下，tensor[i],tensor[i+5],tensor[i+10]出来的hw都一致，64*64/32*32/...
		int *shape = tensors[i].properties.alignedShape.dimensionSize;
		int tensor_c = shape[1];
		int tensor_h = shape[2];
		int tensor_w = shape[3];
		int aligned_hw = tensor_h * tensor_w;

		for (int h = 0; h < tensor_h; h++)
		{
			int offset = h * tensor_w;
			for (int w = 0; w < tensor_w; w++)
			{
				// get score
				int ce_offset = offset + w;
				ce_data[ce_offset] = 1.0 / (1.0 + exp(-ce_data[ce_offset]));

				ScoreId tmp_score = {cls_data[offset + w], 0};
				for (int cls_c = 1; cls_c < tensor_c; cls_c++)
				{
					int cls_index = cls_c * aligned_hw + offset + w;
					if (cls_data[cls_index] > tmp_score.score)
					{
						tmp_score.id = cls_c;
						tmp_score.score = cls_data[cls_index];
					}
				}
				tmp_score.score = 1.0 / (1.0 + exp(-tmp_score.score));
				tmp_score.score = std::sqrt(tmp_score.score * ce_data[ce_offset]);
				if (tmp_score.score <= post_info->score_threshold)
					continue;

				// get detection box
				auto &strides = default_ptq_fcos_config.strides;
				Detection detection;
				detection.bbox.xmin =
					((w + 0.5) * strides[i] - bbox_data[offset + w]) * w_scale;
				detection.bbox.ymin =
					((h + 0.5) * strides[i] - bbox_data[1 * aligned_hw + offset + w]) *
					h_scale;
				detection.bbox.xmax =
					((w + 0.5) * strides[i] + bbox_data[2 * aligned_hw + offset + w]) *
					w_scale;
				detection.bbox.ymax =
					((h + 0.5) * strides[i] + bbox_data[3 * aligned_hw + offset + w]) *
					h_scale;

				detection.score = tmp_score.score;
				detection.id = tmp_score.id;
				detection.class_name = default_ptq_fcos_config.class_names[detection.id].c_str();
				dets.push_back(detection);
			}
		}
	}
}

static void GetBboxAndScoresScaleNHWC(
	hbDNNTensor *tensors,
	FcosPostProcessInfo_t *post_info,
	std::vector<Detection> &dets)
{
	int ori_h = post_info->ori_height;
	int ori_w = post_info->ori_width;
	int input_h = post_info->height;
	int input_w = post_info->width;
	float w_scale;
	float h_scale;
	// preprocess action is pad and resize
	if (post_info->is_pad_resize)
	{
		float scale = ori_h > ori_w ? ori_h : ori_w;
		w_scale = scale / input_w;
		h_scale = scale / input_h;
	}
	else
	{
		w_scale = static_cast<float>(ori_w) / input_w;
		h_scale = static_cast<float>(ori_h) / input_h;
	}

	// fcos stride is {8, 16, 32, 64, 128}
	for (int i = 0; i < 5; i++)
	{
		auto *cls_data = reinterpret_cast<int32_t *>(tensors[i].sysMem[0].virAddr);
		auto *bbox_data =
			reinterpret_cast<int32_t *>(tensors[i + 5].sysMem[0].virAddr);
		auto *ce_data =
			reinterpret_cast<int32_t *>(tensors[i + 10].sysMem[0].virAddr);
		float *cls_scale = tensors[i].properties.scale.scaleData;
		float *bbox_scale = tensors[i + 5].properties.scale.scaleData;
		float *ce_scale = tensors[i + 10].properties.scale.scaleData;

		// 同一个尺度下，tensor[i],tensor[i+5],tensor[i+10]出来的hw都一致，64*64/32*32/...
		int *shape = tensors[i].properties.alignedShape.dimensionSize;
		int tensor_h = shape[1];
		int tensor_w = shape[2];
		int tensor_c = shape[3];
		int32_t bbox_c_stride{
			tensors[i + 5].properties.alignedShape.dimensionSize[3]};
		int32_t ce_c_stride{
			tensors[i + 10].properties.alignedShape.dimensionSize[3]};

		for (int h = 0; h < tensor_h; h++)
		{
			for (int w = 0; w < tensor_w; w++)
			{
				// get score
				int ce_offset = (h * tensor_w + w) * ce_c_stride;
				float ce_data_offset =
					1.0 / (1.0 + exp(-ce_data[ce_offset] * ce_scale[0]));

				int cls_offset = (h * tensor_w + w) * tensor_c;
				ScoreId tmp_score = {cls_data[cls_offset] * cls_scale[0], 0};
				for (int cls_c = 1; cls_c < tensor_c; cls_c++)
				{
					int cls_index = cls_offset + cls_c;
					float score = cls_data[cls_index] * cls_scale[cls_c];
					if (score > tmp_score.score)
					{
						tmp_score.id = cls_c;
						tmp_score.score = score;
					}
				}
				tmp_score.score = 1.0 / (1.0 + exp(-tmp_score.score));
				tmp_score.score = std::sqrt(tmp_score.score * ce_data_offset);
				if (tmp_score.score <= post_info->score_threshold)
					continue;

				// get detection box
				Detection detection;
				int index = bbox_c_stride * (h * tensor_w + w);
				auto &strides = default_ptq_fcos_config.strides;

				detection.bbox.xmin =
					((w + 0.5) * strides[i] - (bbox_data[index] * bbox_scale[0])) *
					w_scale;
				detection.bbox.ymin =
					((h + 0.5) * strides[i] - (bbox_data[index + 1] * bbox_scale[1])) *
					h_scale;
				detection.bbox.xmax =
					((w + 0.5) * strides[i] + (bbox_data[index + 2] * bbox_scale[2])) *
					w_scale;
				detection.bbox.ymax =
					((h + 0.5) * strides[i] + (bbox_data[index + 3] * bbox_scale[3])) *
					h_scale;

				detection.score = tmp_score.score;
				detection.id = tmp_score.id;
				detection.class_name = default_ptq_fcos_config.class_names[detection.id].c_str();
				dets.push_back(detection);
			}
		}
	}
}

static void GetBboxAndScoresScaleNCHW(
	hbDNNTensor *tensors,
	FcosPostProcessInfo_t *post_info,
	std::vector<Detection> &dets)
{
	int ori_h = post_info->ori_height;
	int ori_w = post_info->ori_width;
	int input_h = post_info->height;
	int input_w = post_info->width;
	float w_scale;
	float h_scale;
	// preprocess action is pad and resize
	if (post_info->is_pad_resize)
	{
		float scale = ori_h > ori_w ? ori_h : ori_w;
		w_scale = scale / input_w;
		h_scale = scale / input_h;
	}
	else
	{
		w_scale = static_cast<float>(ori_w) / input_w;
		h_scale = static_cast<float>(ori_h) / input_h;
	}

	/* sqrt(sigmoid(cls) * sigmoid(ce)) <= score_threshold_  equals to
	 ** sigmoid(cls) * sigmoid(ce) <= score_threshold_^2
	 */
	float score_thresh = post_info->score_threshold * post_info->score_threshold;

	/* if cls <= -ln( 1 / score_threshold_^2 -1), then
	** 1 + exp(-cls) >= 1 / score_threshold_^2, then
	** sigmoid(cls) <= score_threshold_^2, then
	** sigmoid(cls) * sigmoid(ce) <= score_threshold_^2
	** use pre_thresh to pre-filter data for ce and cls data
	*/
	float pre_thresh = -log(1 / score_thresh - 1);

	for (int i = 0; i < 5; i++)
	{
		auto *cls_data = reinterpret_cast<int32_t *>(tensors[i].sysMem[0].virAddr);
		float *de_cls = tensors[i].properties.scale.scaleData;

		auto *bbox_data =
			reinterpret_cast<int32_t *>(tensors[i + 5].sysMem[0].virAddr);
		float *de_bbox = tensors[i + 5].properties.scale.scaleData;

		auto *ce_data =
			reinterpret_cast<int32_t *>(tensors[i + 10].sysMem[0].virAddr);
		float *de_ce = tensors[i + 10].properties.scale.scaleData;

		int *shape = tensors[i].properties.alignedShape.dimensionSize;
		int tensor_c = shape[1];
		int tensor_h = shape[2];
		int tensor_w = shape[3];
		int tensor_vw = tensors[i].properties.validShape.dimensionSize[3];
		int aligned_hw = tensor_h * tensor_w;

		ScoreId tmp;

		tmp.score = pre_thresh;
		tmp.id = -1;
		// mask score stored the max score and id in 80 classes for every pixels
		std::vector<std::vector<ScoreId>> mask_score(
			tensor_h, std::vector<ScoreId>(tensor_w, tmp));

		for (int c = 0; c < tensor_c; c++)
		{
			int offset_c = c * aligned_hw;
			for (int h = 0; h < tensor_h; h++)
			{
				int ce_offset_h = h * tensor_w;
				int offset_h = offset_c + ce_offset_h;
				for (int w = 0; w < tensor_vw; w++)
				{
					int score_offset = offset_h + w;
					float tmp_score = cls_data[score_offset] * de_cls[c];
					if (tmp_score <= mask_score[h][w].score)
						continue;
					mask_score[h][w].score = tmp_score;
					mask_score[h][w].id = c;
				}
			}
		}

		auto &strides = default_ptq_fcos_config.strides;

		for (int h = 0; h < tensor_h; h++)
		{
			int ce_offset_h = h * tensor_w;
			for (int w = 0; w < tensor_w; w++)
			{
				// if cls <= -ln( 1 / score_threshold_^2 -1)
				if (mask_score[h][w].score <= pre_thresh)
					continue;
				int offset = ce_offset_h + w;
				float tmp_ce = ce_data[offset] * de_ce[0];
				if (tmp_ce <= pre_thresh)
					continue;
				float ce = 1.0 / (1.0 + exp(-tmp_ce));
				float tmp_score = 1.0 / (1.0 + exp(-mask_score[h][w].score));
				// sigmoid(ce) * sigmoid(cls)
				tmp_score = tmp_score * ce;
				if (tmp_score <= score_thresh)
				{
					continue;
				}

				float xmin = std::max(0.f, bbox_data[offset] * de_bbox[0]);
				float ymin = std::max(0.f, bbox_data[offset + aligned_hw] * de_bbox[1]);
				float xmax =
					std::max(0.f, bbox_data[offset + 2 * aligned_hw] * de_bbox[2]);
				float ymax =
					std::max(0.f, bbox_data[offset + 3 * aligned_hw] * de_bbox[3]);

				Detection detection;
				detection.bbox.xmin = (w + 0.5 - xmin) * strides[i] * w_scale;
				detection.bbox.ymin = (h + 0.5 - ymin) * strides[i] * h_scale;
				detection.bbox.xmax = (w + 0.5 + xmax) * strides[i] * w_scale;
				detection.bbox.ymax = (h + 0.5 + ymax) * strides[i] * h_scale;

				detection.score = std::sqrt(tmp_score);
				detection.id = mask_score[h][w].id;
				detection.class_name = default_ptq_fcos_config.class_names[detection.id].c_str();
				dets.push_back(detection);
			}
		}
	}
}

char *FcosPostProcess(FcosPostProcessInfo_t *post_info)
{
	hbDNNTensor *tensors = post_info->output_tensor;

	std::vector<Detection> dets;
	std::vector<Detection> det_restuls;
	char *str_dets;

	int h_index = 0, w_index = 0, c_index = 0;
	get_tensor_hwc_index(&tensors[0], &h_index, &w_index, &c_index);

	if (tensors[0].properties.tensorLayout == HB_DNN_LAYOUT_NHWC)
	{
		GetBboxAndScoresScaleNHWC(tensors, post_info, dets);
	}
	else if (tensors[0].properties.tensorLayout == HB_DNN_LAYOUT_NCHW)
	{
		GetBboxAndScoresScaleNCHW(tensors, post_info, dets);
	}
	else
	{
		printf("tensor layout error.\n");
	}
	// 计算交并比来合并检测框，传入交并比阈值和返回box数量
	fcos_nms(dets, post_info->nms_threshold, post_info->nms_top_k, det_restuls, false);
	std::stringstream out_string;

	// 算法结果转换成json格式
	out_string << "\"timestamp\": ";
	unsigned long timestamp = post_info->tv.tv_sec * 1000000 + post_info->tv.tv_usec;
	out_string << timestamp;
	out_string << ",\"detection_result\": [";
	for (uint32_t i = 0; i < det_restuls.size(); i++)
	{
		auto det_ret = det_restuls[i];
		out_string << det_ret;
		if (i < det_restuls.size() - 1)
			out_string << ",";
	}
	out_string << "]" << std::endl;

	str_dets = (char *)malloc(out_string.str().length() + 1);
	str_dets[out_string.str().length()] = '\0';
	snprintf(str_dets, out_string.str().length(), "%s", out_string.str().c_str());
	return str_dets;
}
