import BrowserCapabilityDetector from './BrowserCapabilityDetector.js'
import PlayerWrapper from './PlayerWrapper.js';
class DisplayWindow {
	constructor() {
		this.pipelineChannel = -1;

		this.playerIsStarted = false;
		this.player = null;
		this.videoElement = null;

		this.alogResultQueue = [];

		this.videoFps = 0;
		this.algoFps = 0;

		this.streamFirstFrameTimestamp = -1;
	}
}

class DisplayWindowManager {
	constructor() {

		//updateLayout 函数中初始化
		this.displayWindowCountInUsed = 0;
		this.displayWindowCountOnlyShowWindow = 0;
		this.displayWindows = [];

		//init 函数中初始化
		this.userCallbacks = {
			onCaptureVIN: null,
			onCaptureISP: null,
			onCaptureVSE: null
		};
		this.mediaServerIPAddr = null;
		this.browserCapabilities = null;

	}
	init(mediaServerIPAddr, callbacks = {}) {
		//浏览器能力检测
		const detector = new BrowserCapabilityDetector();
		this.browserCapabilities = detector.detectAll();
		console.log('浏览器能力检测结果:', this.browserCapabilities);
		detector.showAll(this.browserCapabilities);

		//服务器IP地址
		this.mediaServerIPAddr = mediaServerIPAddr;
		//回调函数
		this.userCallbacks = callbacks;

		//启动定时任务
		this.startPeriodicRefreshTask();

		//视频显示区域初始化
		var displayContainer = document.getElementById("display_container");
		for (var i = 0; i < 16; i++) {
			var layoutCount = i + 1;
			var layoutDiv = document.createElement("div");	// 创建布局容器

			layoutDiv.id = "layout" + layoutCount;
			layoutDiv.className = "layout";
			displayContainer.appendChild(layoutDiv);
			for (var j = 0; j < layoutCount; j++) {// 循环生成视频容器
				var videoContainer = document.createElement("div"); // 创建视频容器
				videoContainer.className = "video-container";

				// 创建 video 元素
				var video = document.createElement("video");
				video.id = "video" + layoutCount + "_" + (j + 1);
				video.muted = true;
				video.autoplay = true;

				// 创建 视频显示canvas 元素
				var video_render_canvas = document.createElement("canvas");
				video_render_canvas.id = "video_render_canvas" + layoutCount + "_" + (j + 1);
				video_render_canvas.className = "video_render_canvas";

				// 创建 canvas 元素
				var canvas = document.createElement("canvas");
				canvas.id = "canvas" + layoutCount + "_" + (j + 1);
				canvas.className = "canvas";

				// 创建 overlay 元素
				var overlay = document.createElement("div");
				overlay.id = "status" + layoutCount + "_" + (j + 1);
				overlay.className = "overlay";
				overlay.style.display = "block";

				// 创建 overlay_alog 元素
				var overlayAlog = document.createElement("div");
				overlayAlog.id = "alog_result" + layoutCount + "_" + (j + 1);
				overlayAlog.className = "overlay_alog";
				overlayAlog.style.display = "block";

				// 创建抓拍按钮容器
				var captureButtonContainer = document.createElement("div");
				captureButtonContainer.id = "capture_buttons" + layoutCount + "_" + (j + 1);
				captureButtonContainer.className = "capture-button-container";

				// 创建抓拍按钮
				var captureRawButton = this.createCaptureButton("RAW", "Sensor 原始 RAW 图", i + 1, j + 1);
				var captureIspButton = this.createCaptureButton("ISP", "ISP 调校的 YUV 图", i + 1, j + 1);
				var captureVseButton = this.createCaptureButton("VSE", "VSE 处理的 YUV 图", i + 1, j + 1);

				// 将按钮添加到按钮容器中
				captureButtonContainer.appendChild(captureRawButton);
				captureButtonContainer.appendChild(captureIspButton);
				captureButtonContainer.appendChild(captureVseButton);

				// 将 video、canvas、overlay、overlayAlog 添加到 videoContainer 中
				videoContainer.appendChild(video);
				videoContainer.appendChild(video_render_canvas);
				videoContainer.appendChild(canvas);
				videoContainer.appendChild(overlay);
				videoContainer.appendChild(overlayAlog);
				videoContainer.appendChild(captureButtonContainer);

				// 将 videoContainer 添加到布局容器中
				layoutDiv.appendChild(videoContainer);
			}
		}
		displayContainer.addEventListener("click", this.handleCaptureButtonClick.bind(this));

	}

	updateLayout(displayWindowCount, isFourceUpdateWindow) {
		console.log("display window manger recv change event, new window count is :",
				displayWindowCount, "force update:", isFourceUpdateWindow);
		//强制删除：删除旧的窗口
		if(isFourceUpdateWindow){

			let needStopPlayer = false;
			for (let i = 1; i <= this.displayWindowCountInUsed ; i++) {
				const newWindow = this.displayWindows[i - 1];
				if(newWindow.playerIsStarted){
					needStopPlayer = true;
					console.warn(`[${newWindow.pipelineChannel}] display will destroy, but player is started.`);
				}
			}
			/*
				连续点击多次提交时，由于下一次的停止和下一次的启动的顺序不同导致，销毁前可能是启动状态，
				比如:
				1. 第一次连续提交按钮-> 回调函数，调用stopPlayer，正常停止
				2. 第二次连续提交按钮-> 回调函数，调用stopPlayer，不会停止
				3. 第一次提交按钮命令返回-> 回调函数，销毁DisplayWindow 销毁播放器（此时是正常的），再启动播放器（导致步骤4不正常）
				4. 第一次提交按钮命令返回-> 回调函数，销毁DisplayWindow 销毁播放器（此时是不正常的），再启动播放器
			*/

			if(needStopPlayer){
				console.warn("Special case(Probably multiple consecutive clicks to submit):\
When destroying a Display Window, if a player is in the startup state, stop it.");
				this.stopPlayer();
			}

			this.displayWindows = [];
			this.displayWindowCountOnlyShowWindow = 0;
			this.displayWindowCountInUsed = 0;
			for (let i = 1; i <= displayWindowCount; i++) {
				const newWindow = this.createDisplayWindow(i);
				this.displayWindows.push(newWindow);
			}
			this.displayWindowCountOnlyShowWindow = displayWindowCount;
			this.displayWindowCountInUsed = displayWindowCount;

		}else{//非强制：旧窗口不变
			this.displayWindowCountOnlyShowWindow = displayWindowCount; //目前没用
		}

		//1. 关闭所有的布局
		const layouts = document.querySelectorAll('.layout');
		layouts.forEach(layout => {
			layout.style.display = 'none';
		});

		//2. 使能指定的布局：根据窗口的个数 选中 显示那个布局
		if (displayWindowCount === 0) {
			return;			//do nothing
		} else if (displayWindowCount === 1) {
			document.getElementById('layout1').style.display = 'block';
		} else if (displayWindowCount === 2) {
			document.getElementById('layout2').style.display = 'flex';
		} else {
			document.getElementById(`layout${displayWindowCount}`).style.display = 'grid';
		}

		// 3. 显示抓拍按钮：只有相机模式才打开
		var radioButton = document.getElementById("cam_solution");
		// Show or hide capture buttons based on radio button state
		for (let i = 1; i <= displayWindowCount; i++) {
			const captureButtons = document.getElementById(`capture_buttons${displayWindowCount}_${i}`);
			if (captureButtons) {
				captureButtons.style.display = 'flex';
			}

			const captureRawButton = document.getElementById(`capture_raw_${displayWindowCount}_${i}`);
			const captureIspButton = document.getElementById(`capture_isp_${displayWindowCount}_${i}`);
			const captureVseButton = document.getElementById(`capture_vse_${displayWindowCount}_${i}`);

			if (radioButton.checked) {
				if (captureRawButton) captureRawButton.style.display = 'flex';
				if (captureIspButton) captureIspButton.style.display = 'flex';
				if (captureVseButton) captureVseButton.style.display = 'flex';
			} else {
				if (captureRawButton) captureRawButton.style.display = 'none';
				if (captureIspButton) captureIspButton.style.display = 'none';
				if (captureVseButton) captureVseButton.style.display = 'none'; //盒子模式有VSE, 由于回灌模式获取图片需要考虑线程同步，暂时关闭
			}
		}
	}

	handleCaptureButtonClick(event) {
		var target = event.target;
		if (target.classList.contains("capture-button")) {
			var buttonId = target.id;
			switch (true) {
				case buttonId.startsWith("capture_raw"):
					var layoutNum = buttonId.split("_")[2];
					var videoNum = buttonId.split("_")[3];

					if (this.userCallbacks && typeof this.userCallbacks.onCaptureVIN === 'function') {
						this.userCallbacks.onCaptureVIN(videoNum);
					} else {
						console.error("onCaptureVIN is not defined or not a function!, all cb is:", this.userCallbacks);
					}
					console.log("Clicked RAW capture button for layout " + layoutNum + ", video " + videoNum);
					break;
				case buttonId.startsWith("capture_isp"):
					var layoutNum = buttonId.split("_")[2];
					var videoNum = buttonId.split("_")[3];

					if (this.userCallbacks && typeof this.userCallbacks.onCaptureISP === 'function') {
						this.userCallbacks.onCaptureISP(videoNum);
					} else {
						console.error("onCaptureISP is not defined or not a function!", this.userCallbacks);
					}

					console.log("Clicked ISP capture button for layout " + layoutNum + ", video " + videoNum);
					break;
				case buttonId.startsWith("capture_vse"):
					var layoutNum = buttonId.split("_")[2];
					var videoNum = buttonId.split("_")[3];

					if (this.userCallbacks && typeof this.userCallbacks.onCaptureVSE === 'function') {
						this.userCallbacks.onCaptureVSE(videoNum);
					} else {
						console.error("onCaptureVSE is not defined or not a function!", this.userCallbacks);
					}

					console.log("Clicked VSE capture button for layout " + layoutNum + ", video " + videoNum);
					break;
				default:
					break;
			}
		}
	}
	createDisplayWindow(window_index) {

		//DisplayWindow
		let newWindow = new DisplayWindow();
		newWindow.pipelineChannel = window_index;

		//Player
		const userCallbacks = {
			onOpen: (window_index) => {
				// console.error(`[${window_index-1}]'s player is opened.`);
			},
			onRecvedFirstFrame: (window_index, timestamp) => {
				// console.log(`[${window_index-1}]'s player recv first frame, timestamp is ${timestamp}.`);
				let display_window_tmp = this.displayWindows[window_index - 1];
				display_window_tmp.streamFirstFrameTimestamp = timestamp;
			},
			onFirstFrameLoaded:(window_index) =>{
				console.log(`[${window_index-1}]'s loaded first frame.`);
				this.processAlogResult(window_index);
			},
			onStoped: (window_index) => {
				// console.error(`[${window_index-1}]'s player is stoped.`);
			},
			onError: (window_index) => {
				// console.error(`[${window_index-1}]'s player is error.`);
			},
		}
		newWindow.player = new PlayerWrapper(this.browserCapabilities,
				newWindow.pipelineChannel, userCallbacks);
		return newWindow;
	}


	startPlayer(codec_types) {
		console.log("==============> startPlayer: ", this.displayWindowCountInUsed);

		for (let i = 1; i <= this.displayWindowCountInUsed; i++) {

			const display_window = this.displayWindows[i - 1];

			if (display_window.player) {
				display_window.player.stop();
			}
			let codec_type = codec_types[i - 1];
			//H265：拉子码流， H264拉主码流
			const stream_type = ( codec_type === 'h265') ? 'sub1' : 'main';

			let wsUrl = `ws://${this.mediaServerIPAddr}:8080/ch${i - 1}/${stream_type}.live.mp4`;
			display_window.player.init(wsUrl, codec_type,
					this.getPlayerDisplayElementId.bind(this));
			display_window.player.start();
			display_window.playerIsStarted = true;

			//清空相关计数
			this.alogResultQueue = [];
			this.videoFps = 0;
			this.algoFps = 0;
			console.log(`	[${i}/${this.displayWindowCountInUsed}] [codec type:${codec_type}] [url:${wsUrl}`);
		}
		this.switchRenderStatus(true);
	}

	stopPlayer() {
		console.log("==============> stopPlayer: ", this.displayWindowCountInUsed);
		for (let i = 1; i <= this.displayWindowCountInUsed; i++) {
			const display_window = this.displayWindows[i - 1];
			if(display_window.player && display_window.playerIsStarted){
				display_window.player.stop();
				display_window.playerIsStarted = false;
			}else{
				console.log(`${display_window.pipelineChannel}player is not stared, so ignore stopPlayer.`);
			}
		}

		this.switchRenderStatus(false);
	}

	switchRenderStatus(isOpen){
		const status_string = isOpen ?'block': 'none';

		for (let i = 1; i <= this.displayWindowCountInUsed; i++) {
			const fpsOverlay = document.getElementById(`status${this.displayWindowCountInUsed}_${i}`);
			if(fpsOverlay == null){
				console.error(`not found element :status${this.displayWindowCountInUsed}_${i}`);
			}else{
				fpsOverlay.style.display = status_string;
			}

			const classifiAlogResult = document.getElementById(`alog_result${this.displayWindowCountInUsed}_${i}`);
			if(fpsOverlay == null){
				console.error(`not found element :alog_result${this.displayWindowCountInUsed}_${i}`);
			}else{
				classifiAlogResult.style.display = status_string;
			}

			const DetectAlogResult = document.getElementById(`canvas${this.displayWindowCountInUsed}_${i}`);
			if(DetectAlogResult == null){
				console.error(`not found element :canvas${this.displayWindowCountInUsed}_${i}`);
			}else{
				DetectAlogResult.style.display = status_string;
			}

		}
	}

	getPlayerDisplayElementId(pipelineChannel, displayElementType){
		if(displayElementType === "video"){
			return `video${this.displayWindowCountInUsed}_${pipelineChannel}`
		}else if(displayElementType === "canvas"){
			return `video_render_canvas${this.displayWindowCountInUsed}_${pipelineChannel}`
		}else{
			return `unsupport${this.displayWindowCountInUsed}_${pipelineChannel}`
		}
	}

	getDisplayWindowCount() {
		return this.displayWindowCountInUsed;
	}
	updateVideoFrameInfo(message) {
		const display_window = this.displayWindows[message.pipeline - 1];
		if (!display_window) {
			return;
		}
		display_window.videoFps++;
	}
	pushAlogResult(message) {
		// console.log('[', message.pipeline, ']',' recv alog result.');
		const display_window = this.displayWindows[message.pipeline - 1];
		if (!display_window) {
			return;
		}
		display_window.algoFps++;

		const queue = display_window.alogResultQueue;
		if (queue && queue.length >= 100) {
			queue.shift(); // 移除数组的第一个元素
		}
		queue.push(message);
	}

	startPeriodicRefreshTask() {

		setInterval(() => { //定时清除屏幕上画的算法结果
			for (let idx = 1; idx <= this.displayWindowCountInUsed; idx++) {
				const display_window = this.displayWindows[idx - 1];

				if((!display_window.player) || (!display_window.playerIsStarted)){
					continue;
				}

				// 清除画布上的算法渲染信息
				var canvas = document.getElementById(`canvas${this.displayWindowCountInUsed}_${idx}`);
				var context2D = canvas.getContext("2d");
				context2D.clearRect(0, 0, canvas.width, canvas.height);
				context2D.globalAlpha = 50;

				// 构造对应方格的 id
				const alogResultId = `alog_result${this.displayWindowCountInUsed}_${idx}`;
				// 获取对应的 overlay 元素
				const alogResultOverlay = document.getElementById(alogResultId);
				// 填写 overlay 的值
				if (alogResultOverlay) {
					// 在这里填写你要显示的内容，例如：
					alogResultOverlay.textContent = "";
				}
			}
		}, 1000);

		setInterval(() => {//定时刷新视频帧率和算法帧率
			for (let idx = 1; idx <= this.displayWindowCountInUsed; idx++) {
				const display_window = this.displayWindows[idx - 1];
				if((!display_window.player) || (!display_window.playerIsStarted)){
					continue;
				}

				// 构造对应方格的 id
				const overlayId = `status${this.displayWindowCountInUsed}_${idx}`;
				const statusOverlay = document.getElementById(overlayId);
				if (statusOverlay) {
					statusOverlay.textContent = "视频帧率: " + display_window.videoFps + "   " + "算法帧率: " + display_window.algoFps;
				}
				display_window.videoFps = 0;
				display_window.algoFps = 0;
			}
		}, 1000); //1秒刷新一次帧率
	}

	drawClassificationResult(index, classification_result) {
		const alogResultOverlay = document.getElementById(`alog_result${this.displayWindowCountInUsed}_${index}`);
		// 填写 overlay 的值
		var msg = "";
		if (alogResultOverlay) {
			// 解析 msg 获取 id
			const idMatch = classification_result.match(/id=(\d+)/);
			if (idMatch && idMatch[1]) {
				const id = idMatch[1];
				// 调用 get_class_name_by_id 获取 class_name
				const class_name = get_class_name_by_id(String(id));
				// 如果成功获取到 class_name，则补充到 msg 后面
				if (class_name) {
					if (msg != "") {
						msg += ', ';
					}
					msg += `class_name=${class_name}`;
				}
			}
			alogResultOverlay.textContent = "分类算法结果: " + msg;
		}
	}

	drawDetectionResult(index, detection_result) {
		// 获取画布和视频内容的尺寸
		const display_window = this.displayWindows[index - 1];
		const {clientWidth:videoElementClientWidth, clientHeight:videoElementClientHeight} = display_window.player.displayWindowSize();
		const {videoWidth,videoHeight} = display_window.player.VideoResolutionSize();

		// console.log(`display: ${videoElementClientWidth}*${videoElementClientHeight}    video:${videoWidth}*${videoHeight}`);
		var canvas = document.getElementById(`canvas${this.displayWindowCountInUsed}_${index}`);
		var context2D = canvas.getContext("2d");

		// 清空画布
		context2D.clearRect(0, 0, canvas.width, canvas.height);
		context2D.globalAlpha = 50;

		// 设置 canvas 尺寸与 video 尺寸一致
		canvas.width = videoElementClientWidth;
		canvas.height = videoElementClientHeight;


		// 计算 video 的缩放比例
		var scaleX = videoElementClientWidth / videoWidth;
		var scaleY = videoElementClientHeight / videoHeight;

		// 使用最小缩放比例，确保内容保持正确的宽高比
		var scale = Math.min(scaleX, scaleY);

		// 计算视频在画布中的偏移量
		var offsetX = (canvas.width - videoWidth * scale) / 2;
		var offsetY = (canvas.height - videoHeight * scale) / 2;

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


	/**
	 *	视频内容和AI检测结果进行同步
	 * @param {*} index
	 * @returns
	 */
	processAlogResult(index) {

		if (!this.displayWindows[index - 1]) {
			return;
		}
		const display_window = this.displayWindows[index - 1];
		if((!display_window.player) || (!display_window.playerIsStarted)){
			return;
		}

		const streamFirstFrameTimestamp = display_window.streamFirstFrameTimestamp;
		if (streamFirstFrameTimestamp === -1) {
			return;
		}
		var closestElement = null;
		var closestDiff = Infinity;

		var errorTime = 100 * 1000; // 定义误差时间

		// video.currentTime 单位秒， 这个值会跟随 video.playbackRate的设置按倍数增加，作为时间差值会有一点问题
		var curVideoTimestamp = (display_window.player.currentTimeSecond() * 1000000) + parseFloat(streamFirstFrameTimestamp) * 1000; //单位是微秒
		for (var i = 0; i < display_window.alogResultQueue.length; i++) {
			var currElement = display_window.alogResultQueue[i];
			var timeDiff = Math.abs(currElement.timestamp - curVideoTimestamp);

			// console.log("pipeline index:", index, "Element:", i, "currElement.timestamp:", currElement.timestamp, "curVideoTimestamp:",
			// 	curVideoTimestamp, "Time difference:", currElement.timestamp - curVideoTimestamp, "timeDiff:", timeDiff);

			// 情况1（等于 视频时间戳）：如果当前元素的时间戳与目标时间戳相等，则直接选择该元素
			if (timeDiff == 0) {
				closestElement = currElement;
				break; // 跳出循环，因为已经找到了匹配的元素
			}

			// 情况2（大于 视频时间戳）：如果当前元素的时间戳大于目标时间戳，则选择上一个元素（如果存在）作为最接近的元素
			if (currElement.timestamp > curVideoTimestamp) {
				// 如果是队列的第一个元素，则直接选择该元素
				if (i === 0) {
					// 如果当前元素的时间戳与目标时间戳的差值小于100ms，并且比之前的差值更小，则更新最接近的元素和差值
					// 视频和算法的时间戳误差小于100ms时，认为匹配成功
					if (timeDiff <= errorTime)
						closestElement = currElement;
				} else {
					// 否则选择当前元素与前一个元素中与目标时间戳更接近的元素
					var prevTimestamp = display_window.alogResultQueue[i - 1].timestamp;
					var prevDiff = Math.abs(prevTimestamp - curVideoTimestamp);
					closestDiff = (timeDiff < prevDiff) ? timeDiff : prevDiff;
					if (closestDiff <= errorTime)
						closestElement = (timeDiff < prevDiff) ? currElement : display_window.alogResultQueue[i - 1];
				}
				break; // 跳出循环，因为已经找到了最接近的元素
			}

			// 情况3（其他情况）：如果遍历到了队列的最后一个元素，选择该元素作为最接近的元素
			if (i === display_window.alogResultQueue.length - 1) {
				closestElement = display_window.alogResultQueue[i];
			}
		}

		// 渲染算法结果
		if (closestElement) {
			// console.log("pipeline index:", index, "curVideoTimestamp:", curVideoTimestamp, "Closest diff:", closestDiff, "Closest element:", closestElement);
			if (closestElement.classification_result) {
				this.drawClassificationResult(closestElement.pipeline, closestElement.classification_result);
			}
			if (closestElement.detection_result) {
				this.drawDetectionResult(closestElement.pipeline, closestElement.detection_result);
			}
			// 删除已经处理过的元素
			var closestIndex = display_window.alogResultQueue.indexOf(closestElement);
			display_window.alogResultQueue.splice(closestIndex, 1);
		}

		var remainingOptionsAfter = display_window.alogResultQueue.length; // 遍历后的剩余选项数
		// console.log("Remaining options before:", remainingOptionsBefore, "Remaining options after:", remainingOptionsAfter);

		// remainingOptionsAfter 大于 30, 可能存在算法结果过时的情况，遍历，把过时超过一定时间的记录删除
		if (remainingOptionsAfter > 30) {
			for (var i = 0; i < display_window.alogResultQueue.length; i++) {
				var currElement = display_window.alogResultQueue[i];
				var timeDiff = curVideoTimestamp - currElement.timestamp;
				if (timeDiff > 100 * 1000) {
					display_window.alogResultQueue.splice(i, 1);
				}
			}
		}
		// 使用requestAnimationFrame()递归调用自身，以便在下一帧更新时执行
		requestAnimationFrame(() => this.processAlogResult(index));
	}


	// 创建抓拍按钮的函数
	createCaptureButton(mode, tooltip, layoutNum, videoNum) {
		var captureButton = document.createElement("button");
		captureButton.className = "capture-button";
		captureButton.id = "capture_" + mode.toLowerCase() + "_" + layoutNum + "_" + videoNum;
		captureButton.innerHTML = "📸 " + mode + " <span class='tooltip'>" + tooltip + "</span>";
		captureButton.display = "none"
		return captureButton;
	}
}
export default DisplayWindowManager;
