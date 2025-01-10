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

document.write("<script type='text/javascript' src='js/websocket.js'></script>");
document.write("<script type='text/javascript' src='js/wfs/wfs.js'></script>");
document.write("<script type='text/javascript' src='js/jquery.min.js'></script>");
document.write("<script type='text/javascript' src='js/imagenet_id_to_classname.js'></script>");

var g_solution_configs = "";
var g_current_layout = 2;

// 定义一个队列数组
var g_alog_result_queue_array = [];

// json中各个字段的中文名，字段类型，以及可使用的数据
const g_solution_fields = {
	"pipeline_count": {
		chinese_name: "视频通路数",
		type: "maxlimit",
		options: "max_pipeline_count"
	},
	"sensor": {
		chinese_name: "Sensor型号",
		type: "stringlist",
		options: "sensor_list", // Placeholder for sensor options
		value_is_index: false
	},
	"encode_type": {
		chinese_name: "编码类型",
		type: "stringlist",
		options: "codec_type_list",
		value_is_index: true
	},
	"encode_bitrate": {
		chinese_name: "编码码率",
		type: "intarray",
		options: "encode_bit_rate_list"
	},
	"model": {
		chinese_name: "算法模型",
		type: "stringlist",
		options: "model_list", // Placeholder for model options
		value_is_index: false
	},
	"gdc_status": {
		chinese_name: "使能GDC",
		type: "stringlist",
		options: "gdc_status_list",
		value_is_index: true
	},
	"stream": {
		chinese_name: "视频数据流",
		type: "text",
		value: "../test_data/1080P_test.h264" // Placeholder for stream value
	},
	"decode_type": {
		chinese_name: "解码类型",
		type: "stringlist",
		options: "codec_type_list",
		value_is_index: false
	},
	"decode_width": {
		chinese_name: "解码宽度",
		type: "int",
		value: 1920 // Placeholder for decode_width value
	},
	"decode_height": {
		chinese_name: "解码高度",
		type: "int",
		value: 1080 // Placeholder for decode_height value
	},
	"decode_frame_rate": {
		chinese_name: "解码帧率",
		type: "int",
		value: 30 // Placeholder for decode_frame_rate value
	},
	"encode_width": {
		chinese_name: "编码宽度",
		type: "int",
		value: 1920 // Placeholder for encode_width value
	},
	"encode_height": {
		chinese_name: "编码高度",
		type: "int",
		value: 1080 // Placeholder for encode_height value
	},
	"encode_frame_rate": {
		chinese_name: "编码帧率",
		type: "int",
		value: 30 // Placeholder for encode_frame_rate value
	}
};

const REQUEST_TYPES = {
	APP_SWITCH: 1,
	SNAPSHOT: 2,
	START_STREAM: 3,
	STOP_STREAM: 4,
	SYNC_TIME: 5,
	SET_BITRATE: 6,
	GET_CONFIG: 7,
	SAVE_CONFIGS: 8,
	RECOVERY_CONFIGS: 9,
	ALOG_RESULT: 10
};

window.onload = function() {
	// 获取 display_container 元素
	var displayContainer = document.getElementById("display_container");

	for (var i = 0; i < 16; i++) {
		var layout = i + 1;

		// 创建布局容器
		var layoutDiv = document.createElement("div");

		// 设置布局容器的 id
		layoutDiv.id = "layout" + (i + 1);

		// 添加类名
		layoutDiv.className = "layout";

		// 将布局容器添加到 display_container 中
		displayContainer.appendChild(layoutDiv);

		// 循环生成视频容器
		for (var j = 0; j < layout; j++) {
			// 创建视频容器
			var videoContainer = document.createElement("div");
			videoContainer.className = "video-container";

			// 创建 video 元素
			var video = document.createElement("video");
			video.id = "video" + (i + 1) + "_" + (j + 1);
			video.muted = true;
			video.autoplay = true;

			// 创建 canvas 元素
			var canvas = document.createElement("canvas");
			canvas.id = "canvas" + (i + 1) + "_" + (j + 1);
			canvas.className = "canvas";

			// 创建 overlay 元素
			var overlay = document.createElement("div");
			overlay.id = "status" + (i + 1) + "_" + (j + 1);
			overlay.className = "overlay";
			overlay.style.display = "block";

			// 创建 overlay_alog 元素
			var overlayAlog = document.createElement("div");
			overlayAlog.id = "alog_result" + (i + 1) + "_" + (j + 1);
			overlayAlog.className = "overlay_alog";
			overlayAlog.style.display = "block";

			// 创建抓拍按钮容器
			var captureButtonContainer = document.createElement("div");
			captureButtonContainer.id = "capture_buttons" + (i + 1) + "_" + (j + 1);
			captureButtonContainer.className = "capture-button-container";

			// 创建抓拍按钮
			var captureRawButton = createCaptureButton("RAW", "Sensor 原始 RAW 图", i + 1, j + 1);
			var captureIspButton = createCaptureButton("ISP", "ISP 调校的 YUV 图", i + 1, j + 1);
			var captureVseButton = createCaptureButton("VSE", "VSE 处理的 YUV 图", i + 1, j + 1);

			// 将按钮添加到按钮容器中
			captureButtonContainer.appendChild(captureRawButton);
			captureButtonContainer.appendChild(captureIspButton);
			captureButtonContainer.appendChild(captureVseButton);

			// 将 video、canvas、overlay、overlayAlog 添加到 videoContainer 中
			videoContainer.appendChild(video);
			videoContainer.appendChild(canvas);
			videoContainer.appendChild(overlay);
			videoContainer.appendChild(overlayAlog);
			videoContainer.appendChild(captureButtonContainer);

			// 将 videoContainer 添加到布局容器中
			layoutDiv.appendChild(videoContainer);
		}
	}

	// 创建抓拍按钮的函数
	function createCaptureButton(mode, tooltip, layoutNum, videoNum) {
		var captureButton = document.createElement("button");
		captureButton.className = "capture-button";
		captureButton.id = "capture_" + mode.toLowerCase() + "_" + layoutNum + "_" + videoNum;
		captureButton.innerHTML = "📸 " + mode + " <span class='tooltip'>" + tooltip + "</span>";
		captureButton.display = "none"
		return captureButton;
	}

	displayContainer.addEventListener("click", function(event) {
		var target = event.target;
		if (target.classList.contains("capture-button")) {
			var buttonId = target.id;
			switch (true) {
				case buttonId.startsWith("capture_raw"):
					var layoutNum = buttonId.split("_")[2];
					var videoNum = buttonId.split("_")[3];
					console.log("Clicked RAW capture button for layout " + layoutNum + ", video " + videoNum);
					var cmdData = {
						type: 'vin',
						format: 'raw',
						videoNum: videoNum
					};
					ws_send_cmd(REQUEST_TYPES.SNAPSHOT, cmdData); // 抓拍
					break;
				case buttonId.startsWith("capture_isp"):
					var layoutNum = buttonId.split("_")[2];
					var videoNum = buttonId.split("_")[3];
					console.log("Clicked ISP capture button for layout " + layoutNum + ", video " + videoNum);
					var cmdData = {
						type: 'isp',
						format: 'yuv',
						videoNum: videoNum
					};
					ws_send_cmd(REQUEST_TYPES.SNAPSHOT, cmdData); // 抓拍
					break;
				case buttonId.startsWith("capture_vse"):
					var layoutNum = buttonId.split("_")[2];
					var videoNum = buttonId.split("_")[3];
					console.log("Clicked VSE capture button for layout " + layoutNum + ", video " + videoNum);
					var cmdData = {
						type: 'vse',
						format: 'yuv',
						videoNum: videoNum
					};
					ws_send_cmd(REQUEST_TYPES.SNAPSHOT, cmdData); // 抓拍
					break;
				default:
					break;
			}
		}
	});
};

function update_solution_status(solutions_config) {
	// 获取状态显示的DOM元素
	const solution_status = document.getElementById("solution_status");

	// 初始化状态文本
	let status_txt = "<h4>当前方案配置：</h4>";

	// 检查当前方案类型
	const solution_name = solutions_config["solution_name"];
	if (solution_name === 'cam_solution') {
		// 如果是智能摄像机方案
		status_txt += "<strong>智能摄像机：</strong></br>";

		// 获取摄像机方案的信息
		const cam_solution = solutions_config["cam_solution"];
		const pipeline_count = cam_solution["pipeline_count"];
		const max_pipeline_count = cam_solution["max_pipeline_count"];

		let enable_count = 0;
		for (let i = 0; i < max_pipeline_count; i++) {
			if(cam_solution["cam_vpp"][i]["is_enable"] === 0){
				continue;
			}
			enable_count++;
		}
		// 更新状态文本，显示启用的视频路数
		status_txt += `- 接入 ${pipeline_count} 路Sensor</br>`;
		status_txt += `- 启用 ${enable_count} 路Sensor</br>`;

		// 列出启用的传感器型号和算法模型
		status_txt += "<strong>启用的Sensor型号和算法模型：</strong></br>";
		let valid_index = 0;
		for (let i = 0; i < max_pipeline_count; i++) {
			if(cam_solution["cam_vpp"][i]["is_enable"] === 0){
				continue;
			}
			const sensor_model = cam_solution["cam_vpp"][i]["sensor"];
			const algorithm_model = cam_solution["cam_vpp"][i]["model"];
			status_txt += `- 第 ${valid_index+1} 路:`;
			status_txt += `<ul>`;
			status_txt += `<li>Sensor型号：${sensor_model}</li>`;
			status_txt += `<li>算法：${algorithm_model}</li>`;
			status_txt += `</ul>`;
			valid_index++;
		}
		status_txt += "<strong>方案框图（点击放大）</strong></br>";
		status_txt += `<img id="solution_image" src="image/camera-slt.jpg" style="display: block;max-width:100%; max-height:100%" />`;
	} else if (solution_name === 'box_solution') {
		// 如果是智能分析盒方案
		status_txt += `<strong>智能分析盒：</strong></br>`;

		// 获取分析盒方案的信息
		const box_solution = solutions_config["box_solution"];
		const pipeline_count = box_solution["pipeline_count"];

		// 更新状态文本，显示启用的视频路数
		status_txt += `- 启用 ${pipeline_count} 路视频</br>`;

		// 列出编解码的基本分辨率信息和算法模型
		status_txt += "<strong>编解码和算法模型：</strong></br>";
		for (let i = 0; i < pipeline_count; i++) {
			const decode_resolution = `${box_solution["box_vpp"][i]["decode_width"]}
				x${box_solution["box_vpp"][i]["decode_height"]}@
				${box_solution["box_vpp"][i]["decode_frame_rate"]}fps`;

			const encode_resolution = `${box_solution["box_vpp"][i]["encode_width"]}
				x${box_solution["box_vpp"][i]["encode_height"]}@
				${box_solution["box_vpp"][i]["encode_frame_rate"]}fps`;

			const algorithm_model = box_solution["box_vpp"][i]["model"];
			status_txt += `- 第 ${i+1} 路:`;
			status_txt += `<ul>`;
			status_txt += `<li>解码：${decode_resolution}</li>`;
			status_txt += `<li>编码：${encode_resolution}</li>`;
			status_txt += `<li>算法：${algorithm_model}</li>`;
			status_txt += `</ul>`;
		}
		status_txt += "<strong>方案框图（点击放大）</strong></br>";
		status_txt += `<img id="solution_image" src="image/box-slt.jpg" style="display: block;max-width:100%; max-height:100%" />`;
	}

	// 更新状态信息到页面上
	solution_status.innerHTML = status_txt;

	// 添加类以靠左对齐状态消息
	solution_status.classList.add('left-align');

	// 添加点击事件监听器到图片
	document.getElementById("solution_image").addEventListener("click", function() {
		// 创建模态框
		var modal = document.createElement("div");
		modal.style.position = "fixed";
		modal.style.top = "0";
		modal.style.left = "0";
		modal.style.width = "100%";
		modal.style.height = "100%";
		modal.style.backgroundColor = "rgba(0,0,0,0.7)";
		modal.style.zIndex = "1000";
		modal.style.display = "flex";
		modal.style.alignItems = "flex-start";
		modal.style.justifyContent = "center";

		// 创建放大后的图片
		var enlargedImage = document.createElement("img");
		enlargedImage.src = this.src; // 使用点击的图片的 src
		enlargedImage.style.maxWidth = "90%";
		enlargedImage.style.maxHeight = "90%";

		// 添加放大后的图片到模态框中
		modal.appendChild(enlargedImage);

		// 添加关闭按钮到模态框
		var closeButton = document.createElement("button");
		closeButton.textContent = "关闭";
		closeButton.style.marginLeft = "10px"; // 调整关闭按钮的左外边距，使其位于图片右侧
		closeButton.style.padding = "5px 10px";
		closeButton.style.border = "none";
		closeButton.style.backgroundColor = "#ffffff";
		closeButton.style.cursor = "pointer";
		closeButton.addEventListener("click", function() {
			modal.remove(); // 关闭模态框
		});
		modal.appendChild(closeButton);

		// 将模态框添加到页面中
		document.body.appendChild(modal);
	});

}

// 定义一个数组来存储所有视频元素
var videos = [];

// 定义处理视频帧的函数
function processVideoFrame(index) {
	var video = videos[index-1];

	if (video.buffered.length) {
		var end = video.buffered.end(0); //获取当前buffered值
		var diff = end - video.currentTime; //获取buffered与currentTime的差值
		// 差值小于0.3s时根据1倍速进行播放
		if (diff <= 0.3) {
			video.playbackRate = 1;
		}
		// 差值大于0.3s小于5s根据1.2倍速进行播放
		if (diff < 5 && diff > 0.3) {
			video.playbackRate = 1.2;
		}
		if (diff >= 5) {
			console.log("video buffer diff: " + diff);
			//如果差值大于等于5 手动跳帧 这里可根据自身需求来定
			video.currentTime = video.buffered.end(0); //手动跳帧
		}
	}

	return function(timestamp) {
		// 获取当前视频播放时间
		var currentVideoTime = video.currentTime; // 单位秒， 这个值会跟随 video.playbackRate的设置按倍数增加，作为时间差值会有一点问题
		var closestElement = null;
		var closestDiff = Infinity;
		// 定义误差时间
		var errorTime = 100*1000;

		// console.log("pipeline index:", index, "socket.stream_first_timestamp:",
		// 	socket.stream_first_timestamp[index],
		// 	"currentVideoTime", currentVideoTime,
		// 	"timestamp:", timestamp,
		// 	"video.playbackRate:", video.playbackRate);

		var targetTimestamp = (currentVideoTime * 1000000) + parseFloat(socket.stream_first_timestamp[index]); //单位是微秒
		var remainingOptionsBefore = g_alog_result_queue_array[index].length; // 遍历前的剩余选项数

		for (var i = 0; i < g_alog_result_queue_array[index].length; i++) {
			var currElement = g_alog_result_queue_array[index][i];
			var currTimestamp = currElement.timestamp;
			var timeDiff = Math.abs(currTimestamp - targetTimestamp);

			// console.log("pipeline index:", index, "Element:", i, "currTimestamp:", currTimestamp, "targetTimestamp:",
			// 	targetTimestamp, "Time difference:", currTimestamp - targetTimestamp, "timeDiff:", timeDiff);

			// 如果当前元素的时间戳与目标时间戳相等，则直接选择该元素
			if (timeDiff == 0) {
				closestElement = currElement;
				break; // 跳出循环，因为已经找到了匹配的元素
			}

			// 如果当前元素的时间戳大于目标时间戳，则选择上一个元素（如果存在）作为最接近的元素
			if (currTimestamp > targetTimestamp) {
					// 如果是队列的第一个元素，则直接选择该元素
					if (i === 0) {
						// 如果当前元素的时间戳与目标时间戳的差值小于100ms，并且比之前的差值更小，则更新最接近的元素和差值
						// 视频和算法的时间戳误差小于100ms时，认为匹配成功
						if (timeDiff <= errorTime)
							closestElement = currElement;
					} else {
						// 否则选择当前元素与前一个元素中与目标时间戳更接近的元素
						var prevTimestamp = g_alog_result_queue_array[index][i - 1].timestamp;
						var prevDiff = Math.abs(prevTimestamp - targetTimestamp);
						closestDiff = (timeDiff < prevDiff) ? timeDiff : prevDiff;
						if (closestDiff <= errorTime)
							closestElement = (timeDiff < prevDiff) ? currElement : g_alog_result_queue_array[index][i - 1];
					}
					break; // 跳出循环，因为已经找到了最接近的元素
			}

			// 如果遍历到了队列的最后一个元素，选择该元素作为最接近的元素
			if (i === g_alog_result_queue_array[index].length - 1) {
				closestElement = g_alog_result_queue_array[index][i];
			}
		}

		// 渲染算法结果
		if (closestElement) {
			// console.log("pipeline index:", index, "targetTimestamp:", targetTimestamp, "Closest diff:", closestDiff,
			// 	"Closest element:", closestElement);
			if (closestElement.classification_result) {
				show_classification_result(closestElement.pipeline, closestElement.classification_result);
			}
			if (closestElement.detection_result) {
				draw_detection_result(closestElement.pipeline, closestElement.detection_result);
			}
			// 删除已经处理过的元素
			var closestIndex = g_alog_result_queue_array[index].indexOf(closestElement);
			g_alog_result_queue_array[index].splice(closestIndex, 1);
		}

		var remainingOptionsAfter = g_alog_result_queue_array[index].length; // 遍历后的剩余选项数
		// console.log("Remaining options before:", remainingOptionsBefore, "Remaining options after:", remainingOptionsAfter);

		// remainingOptionsAfter 大于 30, 可能存在算法结果过时的情况，遍历，把过时超过一定时间的记录删除
		if (remainingOptionsAfter > 30) {
			for (var i = 0; i < g_alog_result_queue_array[index].length; i++) {
				var currElement = g_alog_result_queue_array[index][i];
				var currTimestamp = currElement.timestamp;
				var timeDiff = targetTimestamp - currTimestamp;
				if (timeDiff > 100 * 1000) {
					g_alog_result_queue_array[index].splice(i, 1);
				}
			}
		}
		// 使用requestAnimationFrame()递归调用自身，以便在下一帧更新时执行
		requestAnimationFrame(processVideoFrame(index));
	};
}

// 当视频准备就绪时
function handleLoadedData(index) {
	return function() {
		// 启动帧更新循环
		requestAnimationFrame(processVideoFrame(index));
	};
}

function open_solution_info(solution) {
	// 根据所选的解决方案，打开相应的HTML页面
	if (solution === "cam_solution") {
		window.open("cam_solution_info.html", "_blank");
	} else if (solution === "box_solution") {
		window.open("box_solution_info.html", "_blank");
	}
}

function adjust_layout(num_videos) {
	const layouts = document.querySelectorAll('.layout');
	layouts.forEach(layout => {
		layout.style.display = 'none';
	});

	if (num_videos === 0) {
		//do nothing
		return;
	} else if (num_videos === 1) {
		document.getElementById('layout1').style.display = 'block';
	} else if (num_videos === 2) {
		document.getElementById('layout2').style.display = 'flex';
	} else {
		document.getElementById(`layout${num_videos}`).style.display = 'grid';
	}

	// 循环遍历每一个视频元素
	// 添加事件监听器
	for (var i = 1; i <= num_videos; i++) {
		var video = document.getElementById(`video${num_videos}_${i}`);
		if (video) {
			// 将视频元素添加到数组中
			videos.push(video);
			// 监听视频准备就绪事件
			video.addEventListener('loadeddata', handleLoadedData(i));
		}
	}

	// 获取单选按钮元素
	var radioButton = document.getElementById("cam_solution");

	// Show or hide capture buttons based on radio button state
	for (let i = 1; i <= num_videos; i++) {
		const captureButtons = document.getElementById(`capture_buttons${num_videos}_${i}`);
		if (captureButtons) {
			captureButtons.style.display = 'flex';
		}

		const captureRawButton = document.getElementById(`capture_raw_${num_videos}_${i}`);
		const captureIspButton = document.getElementById(`capture_isp_${num_videos}_${i}`);
		const captureVseButton = document.getElementById(`capture_vse_${num_videos}_${i}`);

		if (radioButton.checked) {
			if (captureRawButton) captureRawButton.style.display = 'flex';
			if (captureIspButton) captureIspButton.style.display = 'flex';
			if (captureVseButton) captureVseButton.style.display = 'flex';
		} else {
			if (captureRawButton) captureRawButton.style.display = 'none';
			if (captureIspButton) captureIspButton.style.display = 'none';
			if (captureVseButton) captureVseButton.style.display = 'flex';
		}
	}
}

function render_label_name(solutions_config, itemKey, uniqueId, vpp_config) {
	const field = g_solution_fields[itemKey];
	const label = field ? `${field.chinese_name}（${itemKey}）` : itemKey;
	let html = `<li><span>${label}</span>：`;

	const hardware_capability = solutions_config["hardware_capability"];

	if (field) {
		if (field.type === 'stringlist') {
			html += `<select id="${uniqueId}" class="form-control-sm">`;
			const options = hardware_capability[field.options].split('/');
			if(Array.isArray(options)){ //是否是Array
				if(field.value_is_index){//设备返回的值是 int 类型的序号，还是字符串
					let value_index_in_config = vpp_config[itemKey];
					if(value_index_in_config >= options.length){
						console.error("itemKey:" + itemKey + " in hardware_capability:" + field.options + " length is :" + options.length + " config index is :" + value_index_in_config);
						value_index_in_config = 0;
					}
					const selected_value = options[value_index_in_config];
					options.forEach(option => {
						html += `<option value="${option}" ${selected_value === option ? 'selected' : ''}>${option}</option>`;
					});
				}else{
					options.forEach(option => {
						html += `<option value="${option}" ${vpp_config[itemKey] === option ? 'selected' : ''}>${option}</option>`;
					});
				}
			}else{
				console.error("itemKey:" + itemKey + " in hardware_capability:" + field.options + " length is :" + options.length +", and is not a array.");
				const selected_value = hardware_capability[field.options]
				html += `<option value="${selected_value}" selected>${selected_value}</option>`;
			}

			html += `</select>`;
		} else if (field.type === 'intarray') {
			html += `<select id="${uniqueId}" class="form-control-sm">`;
			const options = hardware_capability[field.options];
			options.forEach(option => {
				if (option > 0)
					html += `<option value="${option}" ${vpp_config[itemKey] === option ? 'selected' : ''}>${option}Kbps</option>`;
			});
			html += `</select>`;
		} else if (field.type === 'text') {
			const textValue = vpp_config[itemKey] && vpp_config[itemKey] !== '0' ? vpp_config[itemKey] : field.value;
			html += `<input type="text" id="${uniqueId}" value="${textValue}">`;
		} else if (field.type === 'int') {
			const textValue = vpp_config[itemKey] && vpp_config[itemKey] !== '0' ? vpp_config[itemKey] : field.value;
			html += `<input type="number" id="${uniqueId}" value="${textValue}" step="1">`;
		}
	} else {
		html += `<input type="text" id="${uniqueId}" value="${vpp_config[itemKey]}">`;
	}

	html += `</li>`;
	return html;
}

// 将 JSON 渲染到 HTML 的函数
function render_json_to_html(solutions_config) {
	function getStringDecodeType(decode_type){
		if( decode_type === 0){
			return "h264"; //此处用小写， 为了拼接 vlc字符串
		}else if(decode_type === 1) {
			return "h265";
		}else{
			console.error("recv unsupport decode type:" + decode_type);
			return "unsupport";
		}
	}
	function getIntDecodeType(decode_type_string){
		if( decode_type_string === "H264"){
			return 0;
		}else if(decode_type_string === "H265") {
			return 1;
		}else{
			console.error("recv unsupport decode type:" + decode_type_string);
			return -1;
		}
	}

	const container = document.getElementById('solutionConfig');
	let html = '';

	console.log("solutions_config");

	const solution_name = solutions_config["solution_name"];
	const hardware_capability = solutions_config["hardware_capability"];

	html += `<h2>设备信息</h2>`
	html += `<span style="white-space: pre-wrap;"><strong>芯片类型 : </strong>${hardware_capability["chip_type"]}  </span>`;
	html += `<span style="white-space: pre-wrap;"><strong>软件版本 : </strong>${solutions_config["version"]}  </span>`;
	if (solution_name === 'cam_solution') {
		let valid_index = 0;
		const cam_solution = solutions_config["cam_solution"];
		for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
			const cam_vpp_list = cam_solution.cam_vpp;
			const cam_vpp = cam_vpp_list[i];
			if (cam_vpp.is_enable === 0) {
				continue;
			}
			const codec_type_string = getStringDecodeType(cam_vpp["encode_type"]);
			if(codec_type_string === "unsupport"){
				continue;
			}
			const cameraChannelName = 'CSI_' + cam_vpp.csi_index;
			html += '<br>';
			html += `<span style="white-space: pre-wrap;"><strong>	${cameraChannelName}的码流链接 : </strong>rtsp://${window.location.host}/stream_chn${valid_index}.${codec_type_string}</span>`;
			valid_index++;
		}
	} else if (solution_name === 'box_solution') {
		const box_solution = solutions_config["box_solution"];
		const pipeline_count = box_solution["pipeline_count"];
		const box_vpp_list = box_solution["box_vpp"];
		for (let i = 0; i < pipeline_count; i++) {
			const codec_type_string = getStringDecodeType(box_vpp_list[i]["encode_type"]);
			if(codec_type_string === "unsupport"){
				continue;
			}
			html += '<br>';
			html += `<span style="white-space: pre-wrap;"><strong>	通道${i}的码流链接 : </strong>rtsp://${window.location.host}/stream_chn${i}.${codec_type_string}</span>`;
		}
	}

	html += `<h2>选择应用方案</h2>`
	html += `<form id="solutionForm">`
	html += `<label for="cam_solution">
				<input type="radio" id="cam_solution" name="solution" value="cam_solution"
					${solution_name === "cam_solution" ? 'checked' : ''}> 智能摄像机
				<a href="#" class="info-icon" onclick="open_solution_info('cam_solution')">?</a>
			</label>`;
	html += `<label for="box_solution">
				<input type="radio" id="box_solution" name="solution" value="box_solution"
					${solution_name === "box_solution" ? 'checked' : ''}> 智能分析盒
				<a href="#" class="info-icon" onclick="open_solution_info('box_solution')">?</a>
			</label>`;
	html += `</form>`;

	if (solution_name === 'cam_solution') {
		const cam_solution = solutions_config["cam_solution"];
		// 渲染 cam_vpp
		html += `<div><strong>智能摄像机</strong><ul>`;

		// 渲染 通道开关 复选框
		if(cam_solution.pipeline_count > 0){
			html += '<div><strong>使能Camera接口:</strong><div class="camera_channel_control">';
		}
		const cam_vpp_list = cam_solution.cam_vpp
		const csi_list_info = hardware_capability.csi_list_info;
		// let valid_index = 0; //保证html 相关元素的id从0开始，并且连续
		for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
			const cam_vpp = cam_vpp_list[i];
			if(cam_vpp.is_valid === 0){
				continue;
			}
			const checkboxName = 'CSI_' + cam_vpp.csi_index;
			html += '<div class="checkbox-item">';
			html += '  <input type="checkbox" id="checkbox' + i + '" name="' + checkboxName + '"';
			if (cam_vpp.is_enable) {
				html += ' checked';
			}
			html += '>';
			html += '  <label for="checkbox' + i + '">' + checkboxName + '</label>';
			html += '</div>';
		}
		html += '</div></div>';
		html += '<br>';

		//已经打开的通道的参数
		g_current_layout = 0;
		for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
			const csi_info = csi_list_info.csi_info[i];
			const cam_vpp = cam_vpp_list[i];
			if (cam_vpp.is_enable === 0) {
				continue;
			}
			if(cam_vpp.is_valid === 0){
				console.error('channel:' + i + "is not valid, but cam app is enable:" + cam_vpp.sensor);
				continue;
			}
			g_current_layout += 1;
			html += `<li style="display: inline-block;"><strong>Camera接口(CSI${cam_vpp.csi_index}):</strong><ul>`;
			for (const itemKey in cam_vpp) {
				const uniqueId = `item_${i}_${itemKey}`;
				if(itemKey == "sensor"){
					//sensor 的配置在 hardware_capability.csi_list_info[i].sensor_config_list
					//所以与函数 render_label_name 处理过程不兼容,所以此处复制 一份处理
					const field = g_solution_fields[itemKey];
					const label = field ? `${field.chinese_name}（${itemKey}）` : itemKey;
					html += `<li><span>${label}</span>：`;

					html += `<select id="${uniqueId}" class="form-control-sm">`;
					const options = csi_info.sensor_config_list.split('/');
					options.forEach(option => {
						html += `<option value="${option}" ${cam_vpp["sensor"] === option ? 'selected' : ''}>${option}</option>`;
					});
					html += `</select>`;
					html += `</li>`;
				}else if((itemKey === "csi_index") || (itemKey === "is_enable") || (itemKey === "is_valid") ||  (itemKey === "mclk_is_not_configed") ){
					continue; //不显示

				}else if(itemKey === "gdc_status"){
					if(cam_vpp.gdc_status === -1){
						const textValue = "invalid";
						const field = g_solution_fields[itemKey];
						const label = ` ${field.chinese_name}（${itemKey}）`;
						html += `<li><span>${label}</span>：`;
						html += `<input type="text" id="${uniqueId}" value=" ${textValue}">`;
						html += `</li>`;
					}else{
						html += render_label_name(solutions_config, itemKey, uniqueId, cam_vpp);
					}

				}else{
					html += render_label_name(solutions_config, itemKey, uniqueId, cam_vpp);
				}

			}
			html += `</ul></li>`;
		}
		html += `</ul></div>`;

		//为每个复选框添加事件处理函数
		container.innerHTML = html;
		function createCheckboxChangeHandler(checkboxNumber) {
			return function(event) {
				const cam_solution = g_solution_configs["cam_solution"];
				const cam_vpp_list = cam_solution.cam_vpp
				const cam_vpp = cam_vpp_list[checkboxNumber];
				if(cam_vpp.is_valid === 0){
					console.error("csi_" + cam_vpp.csi_index + " is not valid, but set checkbox." );
					return;
				}
				const isChecked = event.target.checked;
				if (isChecked) {
					cam_vpp.is_enable = 1;
				} else {
					cam_vpp.is_enable = 0;
				}
				render_json_to_html(g_solution_configs);
			};
		}
		for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
			const cam_vpp = cam_vpp_list[i];
			if(cam_vpp.is_valid === 0){
				continue;
			}
			const checkboxName = 'checkbox' + i;
			document.getElementById(checkboxName).addEventListener("change", createCheckboxChangeHandler(i));
		}

		//标签为编码类型的可选框添加事件处理函数
		function encodeTypeChangeHandler(channel_number){
			return function() {
				const cam_solution = g_solution_configs["cam_solution"];
				const cam_vpp_list = cam_solution.cam_vpp
				const cam_vpp = cam_vpp_list[channel_number];
				if(cam_vpp.is_enable === 0){
					console.error("csi_" + cam_vpp.csi_index + " is not enable, but set encodetype." );
					return;
				}
				var selectedEncodeType = this.options[this.selectedIndex].text;
				const encode_type_int = getIntDecodeType(selectedEncodeType);
				if(encode_type_int == -1){
					console.error("csi_" + cam_vpp.csi_index + " recv not support encodetype:" + selectedEncodeType);
					return;
				}

				cam_vpp["encode_type"] = encode_type_int;
				render_json_to_html(g_solution_configs);
			};
		}
		for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
			const cam_vpp = cam_vpp_list[i];
			if (cam_vpp.is_enable === 0) {
				continue;
			}
			const uniqueId = `item_${i}_encode_type`;
			document.getElementById(uniqueId).addEventListener("change", encodeTypeChangeHandler(i));
		}
	} else if (solution_name === 'box_solution') {
		const box_solution = solutions_config["box_solution"];
		g_current_layout = box_solution["pipeline_count"];

		// 渲染 box_vpp
		html += `<div><strong>智能分析盒</strong><ul>`;
		// 渲染 pipeline_count 下拉选择框
		const field = g_solution_fields["pipeline_count"];
		const label = field ? `${field.chinese_name}（pipeline_count）` : "pipeline_count";
		html += `<div"><span>${label}</span>：<select id="item_box_pipeline_count" class="form-control-sm">`;
		for (let option = 1; option <= box_solution[field.options]; option++) {
			html += `<option value="${option}" ${box_solution["pipeline_count"] === option ? 'selected' : ''}>${option}</option>`;
		}
		html += `</select></div>`;

		for (let i = 0; i < box_solution["pipeline_count"]; i++) {
			html += `<li style="display: inline-block;"><strong>第 ${i+1} 路配置：</strong><ul>`;
			for (const itemKey in box_solution["box_vpp"][i]) {
				const uniqueId = `item_${i}_${itemKey}`;
				html += render_label_name(solutions_config, itemKey, uniqueId, box_solution["box_vpp"][i]);
			}
			html += `</ul></li>`;
		}
		html += `</ul></div>`;
		container.innerHTML = html;
		// 添加事件监听器到 pipeline_count 下拉选择框
		document.getElementById("item_box_pipeline_count").addEventListener("change", function () {
			const selectedPipelineCount = parseInt(this.value);
			// 根据选中的 pipeline_count 更新 box_vpp 的显示
			g_solution_configs["box_solution"]["pipeline_count"] = selectedPipelineCount;
			render_json_to_html(g_solution_configs);
		});

		//标签为编码类型的可选框添加事件处理函数
		function encodeTypeBoxSolutionChangeHandler(channel_number){
			return function() {
				const box_solution = solutions_config["box_solution"];
				const box_vpp_list = box_solution.box_vpp
				const box_vpp = box_vpp_list[channel_number];

				var selectedEncodeType = this.options[this.selectedIndex].text;
				const encode_type_int = getIntDecodeType(selectedEncodeType);
				if(encode_type_int == -1){
					console.error("channel:" + channel_number + " recv not support encodetype:" + selectedEncodeType);
					return;
				}
				box_vpp["encode_type"] = encode_type_int;
				render_json_to_html(g_solution_configs);
				};
		}
		for (let i = 0; i < box_solution["pipeline_count"]; i++) {
			const uniqueId = `item_${i}_encode_type`;
			document.getElementById(uniqueId).addEventListener("change", encodeTypeBoxSolutionChangeHandler(i));
		}
	}

	// 调整视频显示格
	adjust_layout(g_current_layout); // 根据实际情况调整参数
	update_solution_status(solutions_config);

	// 添加事件监听器到应用方案选择
	document.getElementById('cam_solution').addEventListener('click', function () {
		const selectedSolution = this.value;
		// 更新选中的解决方案名称
		g_solution_configs["solution_name"] = selectedSolution;
		var solution_image = document.getElementById("solution_image");
		solution_image.style.display = "block";
		solution_image.setAttribute("src", "image/camera-slt.jpg");
		// 重新渲染页面
		render_json_to_html(g_solution_configs);
	});

	document.getElementById('box_solution').addEventListener('click', function () {
		const selectedSolution = this.value;
		// 更新选中的解决方案名称
		g_solution_configs["solution_name"] = selectedSolution;
		var solution_image = document.getElementById("solution_image");
		solution_image.style.display = "block";
		solution_image.setAttribute("src", "image/box-slt.jpg");
		// 重新渲染页面
		render_json_to_html(g_solution_configs);
	});
}

// 页面加载后立即请求连接 websocket 服务器
// 连接成功后，同步时间、获取能力集、调整UI、拉流
var serverIp = window.location.host;
$(document).ready(function () {
	// alert("serverIp:" + serverIp);
	// 链接 websocket 服务器
	// 发起连接
	socket.init("ws://" + serverIp + ":4567")
});

function ws_send_cmd(kind, data) {
	var cmd = {
		kind: kind,
		param: data
	};
	console.log(cmd);
	socket.send(cmd);
}
// 示例用法：
// ws_send_cmd(REQUEST_TYPES.SET_ENCODE_BITRATE, Number(data)); // 设置编码码率

function draw_detection_result(pipeline, detection_result) {
	// 获取画布DOM
	var video = document.getElementById(`video${g_current_layout}_${pipeline}`);
	if (!video) {
		console.log("not found video" + g_current_layout + "_" + pipeline);
		return;
	}

	var canvas = document.getElementById(`canvas${g_current_layout}_${pipeline}`);
	var context2D = canvas.getContext("2d");

	// 清空画布
	context2D.clearRect(0, 0, canvas.width, canvas.height);
	context2D.globalAlpha = 50;

	// 设置 canvas 尺寸与 video 尺寸一致
	canvas.width = video.clientWidth;
	canvas.height = video.clientHeight;

	// 计算 video 的缩放比例
	var scaleX = video.clientWidth / video.videoWidth;
	var scaleY = video.clientHeight / video.videoHeight;

	// 使用最小缩放比例，确保内容保持正确的宽高比
	var scale = Math.min(scaleX, scaleY);

	// 计算视频在画布中的偏移量
	var offsetX = (canvas.width - video.videoWidth * scale) / 2;
	var offsetY = (canvas.height - video.videoHeight * scale) / 2;

	context2D.lineWidth = 2;
	context2D.strokeStyle = "#f1af37";
	context2D.font = "24px Arial";
	context2D.fillStyle = "#ff6666"; // 柔和浅红色

	// 遍历 bbox 并绘制
	for (var i in detection_result) {
		var result = detection_result[i];
		var x = result.bbox[0] * scale + offsetX;
		var y = result.bbox[1] * scale + offsetY;
		var width = (result.bbox[2] - result.bbox[0]) * scale;
		var height = (result.bbox[3] - result.bbox[1]) * scale;

		context2D.strokeRect(x, y, width, height);

		// 绘制标签文本
		var text = result.name ? `${result.name} (${result.score})` : `${result.class_name} (${result.prob})`;
		context2D.fillText(text, x, y - 5); // 在矩形上方显示标签
	}


	context2D.stroke();
	context2D.fill();
}


function show_classification_result(pipeline, msg) {
	const alogResultId = `alog_result${g_current_layout}_${pipeline}`;
	// 获取对应的 overlay 元素
	const alogResultOverlay = document.getElementById(alogResultId);
	// 填写 overlay 的值
	if (alogResultOverlay) {
		// 解析 msg 获取 id
		const idMatch = msg.match(/id=(\d+)/);
		if (idMatch && idMatch[1]) {
			const id = idMatch[1];
			// 调用 get_class_name_by_id 获取 class_name
			const class_name = get_class_name_by_id(String(id));
			// 如果成功获取到 class_name，则补充到 msg 后面
			if (class_name) {
				msg += `, class_name=${class_name}`;
			}
		}
		alogResultOverlay.textContent = "分类算法结果: " + msg;
	}
}

setInterval(() => {
	for (let idx = 1; idx <= g_current_layout; idx++) {
		// 清除画布上的算法渲染信息
		var canvas = document.getElementById(`canvas${g_current_layout}_${idx}`);
		var context2D = canvas.getContext("2d");
		context2D.clearRect(0, 0, canvas.width, canvas.height);
		context2D.globalAlpha = 50;

		// 构造对应方格的 id
		const alogResultId = `alog_result${g_current_layout}_${idx}`;
		// 获取对应的 overlay 元素
		const alogResultOverlay = document.getElementById(alogResultId);
		// 填写 overlay 的值
		if (alogResultOverlay) {
			// 在这里填写你要显示的内容，例如：
			alogResultOverlay.textContent = "";
		}
	}
}, 1000);

setInterval(() => {
	for (let idx = 1; idx <= g_current_layout; idx++) {
		// 构造对应方格的 id
		const overlayId = `status${g_current_layout}_${idx}`;

		// 获取对应的 overlay 元素
		const statusOverlay = document.getElementById(overlayId);

		// 填写 overlay 的值
		if (statusOverlay) {
			statusOverlay.textContent = "视频帧率: " + socket.play_fps[idx] + "   " + "算法帧率: " + socket.smart_fps[idx];
		}

		// 清零播放帧率和算法帧率
		socket.play_fps[idx] = 0;
		socket.smart_fps[idx] = 0;
	}
}, 1000); //1秒刷新一次帧率

function start_stream(chn_count) {
	console.log("start_stream, initialize the queue of algorithm results");
	g_alog_result_queue_array = [];
	for (var i = 1; i <= chn_count; i++) {
		var video = document.getElementById(`video${chn_count}_${i}`);
		var wfs = new Wfs();
		wfs.attachMedia(video, `video${chn_count}_${i}`);
		socket.wfs_handle[i] = wfs;
		// 准备算法结果的队列
		g_alog_result_queue_array[i] = [];
	}
	ws_send_cmd(REQUEST_TYPES.START_STREAM, Number(chn_count)); // 开始推流
}

function stop_stream(chn_count) {
	console.log("stop stream");

	for (var i = 1; i <= chn_count; i++) {
		// 退出拉流
		if (socket.wfs_handle[i]) {
			socket.wfs_handle[i].destroy();
		}
		socket.stream_first_timestamp[i] = -1;
		// 清空算法结果的队列，避免旧数据影响
		g_alog_result_queue_array[i] = [];
	}

	ws_send_cmd(REQUEST_TYPES.STOP_STREAM, Number(chn_count)); // 停止推流
}

// 当视图页面不是激活状态时，退出拉流
// 当再次切回来时，重新拉流
document.addEventListener("visibilitychange", () => {
	console.log("visibilitychange: " + document.hidden);
	if (document.hidden) {
		stop_stream(g_current_layout);
	} else {
		start_stream(g_current_layout);
	}
});

function downloadFile(file_path) {
	try {
		var pos = file_path.lastIndexOf('/');//'/所在的最后位置'
		var file_name = file_path.substr(pos + 1)//截取文件名称字符串
		var urlFile = "http://" + serverIp + "/tmp_file/" + file_name;
		console.log('下载文件:' + urlFile)
		var elemIF = document.createElement("iframe");
		elemIF.src = urlFile;
		elemIF.style.display = "none";
		document.body.appendChild(elemIF);
	} catch (e) {
		console.log('下载文件失败')
	}
}

function show_app_status(msg) {
}

var is_solution_configs_show = 0;
function show_solution_configs() {
	var solution_configs = document.getElementById("solution_configs");
	if (is_solution_configs_show == 0)
		solution_configs.style.display = "table";
	else
		solution_configs.style.display = "none";
	is_solution_configs_show = is_solution_configs_show ? 0 : 1;
}

function update_json_from_html() {
	// 更新 solution_name
	const solution_name = document.querySelector('input[name="solution"]:checked').value;
	g_solution_configs["solution_name"] = solution_name;

	// 更新 cam_solution 或 box_solution 的内容
	if (solution_name === 'cam_solution') {
		const cam_solution = g_solution_configs["cam_solution"];
		// 更新 复选框导致的更新
		// 忽略,在复选框的事件处理函数中已经更新
		// 更新 cam_vpp
		const cam_vpp_list = cam_solution.cam_vpp
		for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
			const cam_vpp = cam_vpp_list[i];
			if(cam_vpp.is_valid === 0){
				continue;
			}
			if (cam_vpp.is_enable === 0) {
				console.log("ignore ->index:" + i);
				continue;
			}
			// console.log("index:" + i);

			for (const itemKey in cam_solution["cam_vpp"][i]) {
				if(itemKey === "csi_index")
					continue;
				if(itemKey === "is_enable")
					continue;
				if(itemKey === "is_valid")
					continue;
				if(itemKey === "mclk_is_not_configed")
					continue;

				if((itemKey === 'gdc_status') && (cam_solution["cam_vpp"][i][itemKey] === -1)){
					continue;
				}

				const uniqueId = `item_${i}_${itemKey}`;
				const element = document.getElementById(uniqueId);
				if (element.tagName === "INPUT") {
					// 根据输入元素的类型更新字段值
					if (element.type === "number") {
						cam_solution["cam_vpp"][i][itemKey] = parseInt(element.value);
					} else {
						cam_solution["cam_vpp"][i][itemKey] = element.value;
					}
				} else if (element.tagName === "SELECT") {
					// 获取选中的选项索引
					const selectedIndex = element.selectedIndex;
					const selectedValue = element.value.trim();
					// 检查选择的值是否是一个有效的数字
					const numericValue = parseInt(selectedValue);
					if (!isNaN(numericValue)) {
						// 如果是数字，则按照数字处理
						// 更新 JSON 中对应字段的值
						cam_solution["cam_vpp"][i][itemKey] = numericValue;
					} else {
						// 如果不是数字，则按照字符串处理
						// 对于 encode_type 或 decode_type，读取下拉选择框的编号值
						if (itemKey === 'encode_type' || itemKey === 'decode_type') {
							cam_solution["cam_vpp"][i][itemKey] = parseInt(selectedIndex);
						}else if (itemKey === 'gdc_status'){
							if(cam_solution["cam_vpp"][i][itemKey] === -1){
							}else{
								cam_solution["cam_vpp"][i][itemKey] = parseInt(selectedIndex);
							}
						}else {
							// 对于其他字段，直接更新为选择的文本值
							cam_solution["cam_vpp"][i][itemKey] = selectedValue;
						}
					}
				}
			}
		}
	} else if (solution_name === 'box_solution') {
		const box_solution = g_solution_configs["box_solution"];
		// 更新 pipeline_count
		const pipelineCountSelect = document.getElementById("item_box_pipeline_count");
		box_solution["pipeline_count"] = parseInt(pipelineCountSelect.value);

		// 更新 box_vpp
		for (let i = 0; i < box_solution["pipeline_count"]; i++) {
			for (const itemKey in box_solution["box_vpp"][i]) {
				const uniqueId = `item_${i}_${itemKey}`;
				const element = document.getElementById(uniqueId);
				if (element.tagName === "INPUT") {
					// 根据输入元素的类型更新字段值
					if (element.type === "number") {
						box_solution["box_vpp"][i][itemKey] = parseInt(element.value);
					} else {
						box_solution["box_vpp"][i][itemKey] = element.value;
					}
				} else if (element.tagName === "SELECT") {
					// 获取选中的选项索引
					const selectedIndex = element.selectedIndex;
					const selectedValue = element.value.trim();
					// 检查选择的值是否是一个有效的数字
					const numericValue = parseInt(selectedValue);
					if (!isNaN(numericValue)) {
						// 如果是数字，则按照数字处理
						// 更新 JSON 中对应字段的值
						box_solution["box_vpp"][i][itemKey] = numericValue;
					} else {
						// 如果不是数字，则按照字符串处理
						// 对于 encode_type 或 decode_type，读取下拉选择框的编号值
						if (itemKey === 'encode_type' || itemKey === 'decode_type') {
							box_solution["box_vpp"][i][itemKey] = parseInt(selectedIndex);
						} else {
							// 对于其他字段，直接更新为选择的文本值
							box_solution["box_vpp"][i][itemKey] = selectedValue;
						}
					}
				}
			}
		}
	}

	// console.log(g_solution_configs);
	update_solution_status(g_solution_configs);
}


function switch_solution() {
	stop_stream(g_current_layout);

	update_json_from_html();

	ws_send_cmd(REQUEST_TYPES.APP_SWITCH, JSON.stringify(g_solution_configs)); // 应用切换
}

function save_solution_configs() {
	update_json_from_html();

	ws_send_cmd(REQUEST_TYPES.SAVE_CONFIGS, JSON.stringify(g_solution_configs)); // 保存配置
}

function recovery_solution_configs() {
	ws_send_cmd(REQUEST_TYPES.RECOVERY_CONFIGS); // 恢复配置
}

function open_video() {
	if (navigator.mediaDevices === undefined) {
		navigator.mediaDevices = {};
	}
	//
	if (navigator.mediaDevices.getUserMedia === undefined) {
		navigator.mediaDevices.getUserMedia = function (constraints) {
			var getUserMedia = navigator.webkitGetUserMedia || navigator.mozGetUserMedia;
			if (!getUserMedia) {
				return Promise.reject(new Error('getUserMedia is not implemented in this browser'));
			}
			return new Promise(function (resolve, reject) {
				getUserMedia.call(navigator, constraints, resolve, reject);
			});
		}
	}

	window.URL = (window.URL || window.webkitURL || window.mozURL || window.msURL);
	var mediaOpts = {
		audio: false,
		video: true,
	}
	function errorFunc(err) {
		alert(err.name);
	}

	navigator.mediaDevices.getUserMedia(mediaOpts, successFunc, errorFunc);
}

// websocket 连接成功
function ws_onopen() {
	// sdb 板子上没有rtc保存时间，所以把pc的时间同步到设备上，让osd时间和pc时间同步
	var currentTime = new Date().getTime() / 1000;
	console.log("currentTime:", currentTime);
	ws_send_cmd(REQUEST_TYPES.SYNC_TIME, Number(currentTime)); // 时间同步

	// 请求类型 kind 7 获取设备信息，如软件版本、芯片类型等
	// 之后接收到设备能力信息后，根据能力集信息进行UI显示调整
	ws_send_cmd(REQUEST_TYPES.GET_CONFIG); // 获取配置
}

function handle_ws_recv(params) {
	// console.log(params);
	if (params.kind == REQUEST_TYPES.APP_SWITCH && params.Status == 200) {
		if (Wfs.isSupported()) {
			start_stream(g_current_layout);
		}
	} else if (params.kind == REQUEST_TYPES.APP_SWITCH && params.app_status) {
		show_app_status(params.app_status);
	} else if (params.kind == REQUEST_TYPES.SNAPSHOT) {
		downloadFile(params.Filename);
	} else if (params.kind == REQUEST_TYPES.ALOG_RESULT) {
		// console.log("Input", params);
		if (params.classification_result) {
			socket.smart_fps[params.pipeline]++;
		}
		if (params.detection_result) {
			socket.smart_fps[params.pipeline]++;
		}
		if (!g_alog_result_queue_array[params.pipeline]) {
			g_alog_result_queue_array[params.pipeline] = []; // 如果不存在，创建一个空数组
		}
		// 将 params 放入相应的队列中
		g_alog_result_queue_array[params.pipeline].push(params);
	} else if (params.kind == REQUEST_TYPES.GET_CONFIG) {
		if (params.solution_configs) {
			g_solution_configs = params.solution_configs;
			render_json_to_html(g_solution_configs);
		}
		// 完成UI界面渲染和调整后再拉流
		// 保证首次拉流时websocket已经连接
		if (Wfs.isSupported()) {
			start_stream(g_current_layout);
		}
	}
}
