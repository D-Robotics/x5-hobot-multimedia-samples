
// import { DecoderRecv, DecoderSend } from "./DecoderCmd.js";
// import { LiveStreamPullerRecv, LiveStreamPullerSent } from "./LiveStreamPullerCmd.js"
const currentScriptUrl = import.meta.url;
const currentScriptDir = new URL("./", currentScriptUrl).href;
// console.log(`当前脚本路径：${currentScriptUrl}`, `目录：${currentScriptDir}`);


const DecoderState = Object.freeze({
	Idle: 0,
	Initializing: 1,
	Ready: 2,
	Finished: 3
});

const PlayerState = Object.freeze({
	Idle: 0,
	Playing: 1,
});

class VideoParam {
	constructor(width, height, codecType, format) {
		this.width = width;
		this.height = height;
		this.codecType = codecType;
		this.format = format
	}
}

class WasmVideoPlayer {
	constructor(wsUrl, canvasElementId, mimeType) {
		//参数
		this.wsUrl = wsUrl;
		this.canvasElementId = canvasElementId;
		this.mimeType = mimeType;

		this.canvasElement = null;

		//拉流器、解码器、显示器
		this.webGLDisplayer = null;
		this.liveStreamWorker = null;
		this.decoderWorker = null;

		//播放器和解码器的状态
		this.decoderState = DecoderState.Idle;
		this.playerState = PlayerState.Idle;

		//拉流器获取到的视频参数
		this.videoParam = null;

		this.decodeInternal = 10 /*ms*/; // 解码间隔
		this.streamReceivedLen = 0;
		this.streamDataCacheLen = 1024;  // 缓存一段裸码流后，再打开解码器

		this.frameBuffer = [];			 // 视频帧队列：解码器和显示器之间数据交互

		this.userCallbacks = {
			onOpen: null,
			onRecvedFirstFrame: null,
			onFirstFrameLoaded: null,
			onStoped: null,
			onError: null
		};

		//附属的显示窗口
		this.attachDisplayElementId = null;
		this.attachDisplayElement = null;

		//标志位
		this.isFirstFrameLoaded = false;
		this.isFirstFrameParsed = false;

		//时间戳
		this.firstFrameTimeStamp = -1;
		this.currentFrameTimeStamp = -1;

		this.initWebgl();
		this.initDecodeWorker();
		this.initLiveStreamWorker();

	}
	init(userCallbacks) {
		this.userCallbacks = userCallbacks;
		this.canvasElement = document.getElementById(this.canvasElementId);
		if (!this.canvasElement) {
			throw new Error("No Canvas with id " + this.canvasElementId + "!");
		}
	}
	initAttachDisplayWindowInfo(attachDisplayElementId){

		this.attachDisplayElement = document.getElementById(attachDisplayElementId);
		if (!this.attachDisplayElement) {
			throw new Error("No Canvas with id " + this.attachDisplayElementId + "!");
		}
		this.attachDisplayElementId = attachDisplayElementId;
	}
	initWebgl() {
		//do nothing
	}

	initLiveStreamWorker() {
		var self = this;
		this.liveStreamWorker = new Worker(`${currentScriptDir}/LiveStreamPuller.js`, {
			name: 'LiveStreamPuller'
		});

		this.liveStreamWorker.onmessage = function (evt) {
			var objData = evt.data;
			switch (objData.t) {
				case LiveStreamPullerSent.CreateRsp:
					break;
				case LiveStreamPullerSent.StartRsp:
					break;
				case LiveStreamPullerSent.StopRsp:
					break;
				case LiveStreamPullerSent.DestroyRsp:
					break;
				case LiveStreamPullerSent.FrameInfo:
					self.onLiveStreamInfo(objData.v);
					break;
				case LiveStreamPullerSent.FrameData:
					self.onLiveStreamData(objData.d, objData.s);
					break;
				case LiveStreamPullerSent.Error:
					break;
			}
		}
		this.liveStreamWorker.onerror = function(error) {
			console.error('LiveStreamPuller error:', error.filename, error.lineno, error.message);
		};
	}

	initDecodeWorker() {
		var self = this;
		console.log("WasmDecoder.js");
		this.decodeWorker = new Worker(`${currentScriptDir}/WasmDecoder.js`, {
			name: 'VideoDecoderWorker'
		});

		this.decodeWorker.onmessage = function (evt) {
			var objData = evt.data;
			switch (objData.t) {
				case DecoderSend.kInitDecoderRsp:
					break;
				case DecoderSend.kOpenDecoderRsp:
					self.onOpenDecoder(objData);

					break;
				case DecoderSend.kVideoFrame:
					self.onVideoFrame(objData);
					break;
			}
		}
		this.decodeWorker.onerror = function(error) {
			console.error('VideoDecoderWorker error:', error.filename, error.lineno, error.message);
		};

	}
	start() {
		this.webGLDisplayer = new WebGLPlayer(this.canvasElement);

		//解码器: 初始化 (延迟打开)
		var req = {
			t: DecoderRecv.kInitDecoderReq,
		};
		this.decodeWorker.postMessage(req);

		//拉流器：启动
		req = {
			t: LiveStreamPullerRecv.StartReq,
			u: this.wsUrl,
		};
		this.liveStreamWorker.postMessage(req);

		this.playerState = PlayerState.Playing;
		this.displayStartAndLoop();

		this.canvasElement.style.display = "block";

		return { success: true, message: "Video player started successfully." };
	}
	stop() {
		console.log("[WasmVideoPlayer] stop .");
		// 1. 停止拉流工作线程
		if (this.liveStreamWorker) {
			const stopReq = {
				t: LiveStreamPullerRecv.StopReq
			};
			this.liveStreamWorker.postMessage(stopReq);
		}

		// 2. 停止解码工作线程
		if (this.decodeWorker) {
			const closeDecoderReq = {
				t: DecoderRecv.kCloseDecoderReq
			};
			this.decodeWorker.postMessage(closeDecoderReq);
		}

		// 3. 停止播放循环
		this.playerState = PlayerState.Idle;
		const terminateWorker = (worker) => {
            if (!worker) return;
            try {
                worker.terminate();
            } catch (e) {
                console.warn("Worker termination error:", e);
            }
        };
        terminateWorker(this.decodeWorker);
        terminateWorker(this.liveStreamWorker);

		// 4. 清空帧缓存
		this.playerState = PlayerState.Idle;

		this.frameBuffer = [];
		this.streamReceivedLen = 0;
		this.decoderState = DecoderState.Idle;

		if (this.canvasElement) {
			const ctx = this.canvasElement.getContext('2d');
			if (ctx) {
				console.log("clearRect");
				ctx.clearRect(0, 0, this.canvasElement.width, this.canvasElement.height);
			}
		}

		// 5. 清除WebGL显示
		if (this.webGLDisplayer && this.webGLDisplayer.destroy) {
			this.webGLDisplayer.destroy();
			console.log("destroy");
		}
		this.canvasElement.style.display = "none";

		// 7. 重置标志位
		this.isFirstFrameLoaded = false;
		this.isFirstFrameParsed = false;
		this.firstFrameTimeStamp = -1;

		// 8. 通知回调
		if (this.userCallbacks && typeof this.userCallbacks.onStoped === 'function') {
			this.userCallbacks.onStoped();
		}


        // 清除引用
        this.decodeWorker = null;
        this.liveStreamWorker = null;
        this.webGLDisplayer = null;
        this.canvasElement = null;

		return { success: true, message: "Video player stopped successfully." };
	}

	currentTimeSecond() {
		return this.currentFrameTimeStamp / 1000;
	}

	displayWindowSize() {
		return {
			clientWidth: this.canvasElement.width,
			clientHeight: this.canvasElement.height,
		};
	}

	VideoResolutionSize() {
		return {
			videoWidth: this.videoParam.width,
			videoHeight: this.videoParam.height,
		};
	}

	/////////////////////////////////////////////////////////////////////////////
	displayStartAndLoop() {
		if (this.playerState != PlayerState.Idle) {
			requestAnimationFrame(this.displayStartAndLoop.bind(this));
		}

		if (this.frameBuffer.length == 0) {
			return;
		}
		if (this.playerState != PlayerState.Playing) {
			return;
		}
		// requestAnimationFrame may be 60fps, if stream fps too large,
		// we need to render more frames in one loop, otherwise display
		// fps won't catch up with source fps, leads to memory increasing,
		// set to 2 now.
		for (let i = 0; i < 2; ++i) {
			var frame = this.frameBuffer[0];
			switch (frame.t) {
				case DecoderSend.kAudioFrame:
					// if (this.displayAudioFrame(frame)) {
					// 	this.frameBuffer.shift();
					// }

					//ignore audio
					this.frameBuffer.shift();
					break;
				case DecoderSend.kVideoFrame:
					if (this.displayVideoFrame(frame)) {
						this.frameBuffer.shift();
					}
					break;
				default:
					return;
			}

			if (this.frameBuffer.length == 0) {
				break;
			}
		}

	}

	displayVideoFrame(frame) {
		if (this.playerState != PlayerState.Playing) {
			return false;
		}
		if(!this.isFirstFrameLoaded){
			this.isFirstFrameLoaded = true;
			if (this.userCallbacks &&
				typeof this.userCallbacks.onFirstFrameLoaded === 'function') {
					this.userCallbacks.onFirstFrameLoaded();
			}
		}

		this.currentFrameTimeStamp = frame.s;

		var data = new Uint8Array(frame.d);
		this.webGLDisplayer.renderFrame(data);
		return true;
	};


	reportPlayError(error, status, message) {
		var e = {
			error: error || 0,
			status: status || 0,
			message: message
		};
		// this.userCallback(e);
	};

	/////////////////////////////////////////////////////////////////
	onLiveStreamInfo(v) {
		console.log(`recvive stream data info ${v.w}*${v.h}.`);
		this.videoParam = new VideoParam(v.w, v.h, v.c, v.p);
		this.webGLDisplayer.initVideoParam(v.w, v.h, v.p);

		this.adjustDisplayWindowSizeDynamically();
	}
	onLiveStreamData(streamData, dts) {

		if (this.playerState != PlayerState.Playing) {
			return;
		}
		if(!this.isFirstFrameParsed){
			this.isFirstFrameParsed = true;
			this.firstFrameTimeStamp = dts;
			if (this.userCallbacks &&
				typeof this.userCallbacks.onRecvedFirstFrame === 'function') {
					this.userCallbacks.onRecvedFirstFrame(dts);
			}
		}

		var dataLength = streamData.byteLength;
		var objData = {
			t: DecoderRecv.kFeedDataReq,
			d: streamData.buffer,
			s: dts
		};
		//注意：[objData.d]的操作会把 streamData.buffer 转移，所以 dataLength 要提前保存
		this.decodeWorker.postMessage(objData, [objData.d]);

		//缓存一段视频后再打开播放器
		if (this.decoderState == DecoderState.Idle) {
			this.streamReceivedLen += dataLength;

			if (this.streamReceivedLen >= this.streamDataCacheLen) {

				//打开解码器：（解码器打开成功后，启动解码器）
				this.decoderState = DecoderState.Initializing;
				var objData = {
					t: DecoderRecv.kOpenDecoderReq,
					v: {
						w: this.videoParam.width,
						h: this.videoParam.height,
						c: this.videoParam.codecType,
						p: this.videoParam.format
					}
				};
				this.decodeWorker.postMessage(objData);
			} else {
				//在解码器中缓存数据
			}
		}
	}

	/////////////////////////////////////////////////////////////////
	onOpenDecoder(objData) {
		if (objData.e == 0) {
			this.decoderState = DecoderState.Ready;
			this.startDecoding();
			if (this.userCallbacks &&
				typeof this.userCallbacks.onOpen === 'function') {
					this.userCallbacks.onOpen();
			}

		} else {
			console.error("open decoder failed" + objData.e);
			// this.userCallbacks.onError()
		}
	}
	onVideoFrame(frame) {
		if(this.playerState != PlayerState.Idle){
			this.frameBuffer.push(frame);
		}

		if (this.frameBuffer.length >= 3) {
			this.frameBuffer.shift(); // 丢弃最早的一帧
		}
	};


	startDecoding() {
		console.log("start decode.")
		var req = {
			t: DecoderRecv.kStartDecodingReq,
			i: this.decodeInternal,
		};
		this.decodeWorker.postMessage(req);
		this.decoding = true;
	};

	pauseDecoding() {
		var req = {
			t: kPauseDecodingReq
		};
		this.decodeWorker.postMessage(req);
		this.decoding = false;
	};
	/////////////////////////////////////////////////////////////

	adjustAttachDisplayWindowSizeDynamically(){
		if(this.attachDisplayElement === null){
			return;
		}

		const aspectRatio = this.videoParam.width / this.videoParam.height;
		const containerWidth = this.attachDisplayElement.clientWidth;

		this.attachDisplayElement.style.width = `${containerWidth}px`;
		this.attachDisplayElement.style.height = `${containerWidth / aspectRatio}px`;
	}

	adjustDisplayWindowSizeDynamically(){

		if((this.videoParam == null) || (this.videoParam.width <= 0) || (this.videoParam.height <= 0)){
			return;
		}

		this.adjustAttachDisplayWindowSizeDynamically();

		const canvasElement = this.canvasElement;
		const mediaWidth = this.videoParam.width;
		const mediaHeight = this.videoParam.height;
		if (!canvasElement || !canvasElement.parentElement) return;

		const canvasElementContainer = canvasElement.parentElement;
		const videoRatio = mediaWidth / mediaHeight;
		const canvasElementContainerRatio = canvasElementContainer.clientWidth / canvasElementContainer.clientHeight;

		// 计算显示尺寸（保持宽高比）
		let displayWidth, displayHeight;
		if (videoRatio > canvasElementContainerRatio) {
			// 视频更宽 => 宽度撑满容器
			displayWidth = canvasElementContainer.clientWidth;
			displayHeight = displayWidth / videoRatio;
		} else {
			// 视频更高 => 高度撑满容器
			displayHeight = canvasElementContainer.clientHeight;
			displayWidth = displayHeight * videoRatio;
		}

		// 更新画布显示尺寸（CSS像素）
		canvasElement.style.width = `${mediaWidth}px`;
		canvasElement.style.height = `${mediaHeight}px`;

		// 保持原生分辨率（避免模糊）
		canvasElement.width = mediaWidth;
		canvasElement.height = mediaHeight;

		// 二次校验比例（防小数点误差）
		const actualRatio = canvasElement.clientWidth / canvasElement.clientHeight;
		if (Math.abs(videoRatio - actualRatio) > 0.01) {
			// 强制修正（高度优先）
			const correctedHeight = canvasElementContainer.clientHeight;
			const correctedWidth = correctedHeight * videoRatio;

			canvasElement.style.width = `${mediaWidth}px`;
			canvasElement.style.height = `${mediaHeight}px`;
			// console.warn('比例修正:', `${correctedWidth.toFixed(0)}x${correctedHeight.toFixed(0)}`);
		}

		console.log('最终显示尺寸:', {
			cssSize: `${displayWidth.toFixed(0)}x${displayHeight.toFixed(0)}`,
			resolution: `${mediaWidth}x${mediaHeight},`,
			parent: `${canvasElementContainer.clientWidth}x${canvasElementContainer.clientHeight},`
		});
	};
};
export default WasmVideoPlayer;


// const container = this.canvasElement.parentElement || document.body;
// 		const containerWidth = container.clientWidth;


// 		// 保持视频原始宽高比
// 		const aspectRatio = this.videoParam.width / this.videoParam.height;
// 		const height = containerWidth / aspectRatio;


// 		this.canvasElement.width = containerWidth;
// 		this.canvasElement.height = height;

// 		// 设置CSS尺寸
// 		this.canvasElement.style.width = '100%';
// 		this.canvasElement.style.height = 'auto';


// 		console.log("adjust:", this.canvasElement.width, " this.canvasElement.height:", this.canvasElement.height);