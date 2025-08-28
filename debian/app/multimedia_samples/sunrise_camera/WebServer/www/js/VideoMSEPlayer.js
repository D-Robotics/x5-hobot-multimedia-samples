// import { MP4Box } from "./plugin-in/mp4box.all.min";

class VideoMSEPlayer {
    constructor(wsUrl, videoElementId, videoCodecType = 'avc1.640028') {

		//websocket
		this.ws = null;
        this.wsUrl = wsUrl;
		this.streamDataQueue = [];		// StreamData 缓存

		//display
		this.videoElement = null;
		this.preVideoPlayPos = 0;			//上次播放器播放的位置
        this.videoElementId = videoElementId;

		//MSE 相关
		this.isSourceOpen = false;
        this.mediaSource = null;	// MSE 解码器
        this.sourceBuffer = null;	// MSE 解码器 对应的 数据源
		this.videoCodecType = videoCodecType; //MSE 需要指定解码器

		//时间戳相关
		this.isFirstFrameParsed = false;
		this.firstFrameTimeStamp = -1;
		this.mp4boxFile = null;
		this.currentOffset = 0;

		this.userCallbacks = {
			onOpen: null,
			onRecvedFirstFrame: null,
			onFirstFrameLoaded: null,
			onStoped: null,
			onError: null
		};

		this.isStart = false;
    }

	init(userCallbacks){
		this.userCallbacks = userCallbacks;
	}

    start() {

        if (this.ws || this.mediaSource) {
            const warningMsg = "Player is already running.";
            console.warn(warningMsg);
            return { success: false, message: warningMsg };
        }

        this.videoElement = document.getElementById(this.videoElementId);
        if (!this.videoElement) {
            const errorMsg = `Video element with ID "${this.videoElementId}" not found.`;
            console.error(errorMsg);
            return { success: false, message: errorMsg };
        }
		this.videoElement.addEventListener('loadeddata', () => this.userCallbacks.onFirstFrameLoaded());

        try {

            this.mediaSource = new MediaSource(); //实现分段加载，<video>必须把文件加载完毕才可以播放
            this.mediaSource.addEventListener("sourceopen", this._onSourceOpen.bind(this));
			this.mediaSource.addEventListener('sourceended', () => console.log("MediaSource ended"));
			this.mediaSource.addEventListener('sourceclose', () => console.log("MediaSource closed"));
			this.mediaSource.addEventListener('error', (e) => {
				console.log('SourceBuffer错误:', e.message);
			  });


            this.videoElement.src = URL.createObjectURL(this.mediaSource);

			this._initWebSocket();
			this.mp4boxFile = MP4Box.createFile();
			this.mp4boxFile.onReady = this.handleReady.bind(this);

			this.isStart = true;
            return { success: true, message: "Video player started successfully." };
        } catch (error) {
            console.error("Failed to initialize MediaSource:", error.message);
            return { success: false, message: `Failed to initialize MediaSource: ${error.message}` };
        }
    }

    stop() {
		this.isStart = false;
		try {
			// 1. 清理 videoElement 并撤销 Blob URL
			if (this.videoElement) {
				const oldSrc = this.videoElement.src;
				this.videoElement.src = "";
				this.videoElement.load();

				// 手动撤销 Blob URL（如果是 blob: 协议）
				if (oldSrc.startsWith("blob:")) {
					URL.revokeObjectURL(oldSrc);
				}
			}

			// 2. 清理 MediaSource
			if (this.mediaSource) {
				if (this.mediaSource.readyState === "open" && this.sourceBuffer) {
					if (this.sourceBuffer.updating) {
						this.sourceBuffer.abort();  // 强制终止未完成操作
					}
					this.mediaSource.endOfStream();  // 确保缓冲区关闭
				}
				this.mediaSource.removeEventListener("sourceopen", this._onSourceOpen);
				this.mediaSource = null;
			}

			// 3. 清理 WebSocket
			if (this.ws) {
				this.ws.close();
				this.ws = null;
			}

			// 4. 重置其他状态
			this.streamDataQueue = [];
			this.isSourceOpen = false;
			this.sourceBuffer = null;
			console.log("Video player stopped successfully.");
			return { success: true, message: "Stopped successfully." };
		} catch (error) {
			console.error("Failed to stop:", error);
			return { success: false, message: `Failed to stop: ${error.message}` };
		}
	}

	currentTimeSecond(){
		return this.videoElement.currentTime;
	}

	displayWindowSize(){
		return {
			clientWidth : this.videoElement.clientWidth,
			clientHeight : this.videoElement.clientHeight,
		};
	}
	VideoResolutionSize(){
		return {
			videoWidth : this.videoElement.videoWidth,
			videoHeight : this.videoElement.videoHeight,
		};
	}

	///////////////////////////////////////////////////////////////////////////////////
    _onSourceOpen() {

		//特殊情况：只有edge 在某个页面首次打开时，会触发如下逻辑，如果添加了播放器能力检测逻辑，下面的代码不会再触发
		if(this.mediaSource.readyState != 'open'){
			this.isSourceOpen = false;
			/**
			 * 问题：edge 浏览器使用MSE解码时，第一次打开后，很快会再调用 _onSourceOpen 而且状态是closed
			 * 解决：重启播放器
			 */
			if(this.isStart = true){
				console.log("\n\n");
				console.warn(`Player is started, but recv closed event(${this.mediaSource.readyState}), so restart it.`);
				this.stop();
				this.start();
				console.warn("restart end \n\n");
			}
			return;
		}

        try {
            // this.sourceBuffer = this.mediaSource.addSourceBuffer(`video/mp4; codecs="${this.videoCodecType},mp4a.40.="`);

			let mime =`video/mp4; codecs="${this.videoCodecType}"`;
			this.sourceBuffer = this.mediaSource.addSourceBuffer(mime);
            this.sourceBuffer.addEventListener("updateend", this._onUpdateEnd.bind(this));
            console.log(`MediaSource and SourceBuffer${mime} initialized successfully.`);
			this.isSourceOpen = true;
        } catch (error) {
            console.error("Failed to initialize SourceBuffer:", error.message);
			this.isSourceOpen = false;
			this.userCallbacks.onError({ success: false, message: `Failed to initialize SourceBuffer: ${error.message}` });
        }
    }
	/**
	 * 播放器更新有两种逻辑：
	 * 	1. 设置 快进速度
	 *  2. 指定 播放位置
	 *
	 * 由于播放器使用的 时间单位是秒为单位的，所以只能保证误差在 秒 级别
	 */

	//当缓冲区更新完成时触发： 基于播放速率实现 快进和快退的逻辑
    _onUpdateEnd() {
        // const { currentTime, buffered } = this.videoElement;
        if (this.videoElement.buffered.length <= 0) return;

		var end = this.videoElement.buffered.end(0); //获取当前buffered值
		var diff = end - this.videoElement.currentTime; //获取buffered与currentTime的差值
		// 差值小于0.3s时根据1倍速进行播放
		if (diff <= 0.2) {
			this.videoElement.playbackRate = 1;
		}
		// 差值大于0.3s小于5s根据1.5倍速进行播放
		if (diff < 5 && diff > 0.2) {
			this.videoElement.playbackRate = 1.5;
			// console.log("MSEPlayer change playbackRate:", this.videoElement.playbackRate);
		}
		if (diff >= 5) {
			console.warn("MSEPlayer found video buffer diff too long:" + diff);
			//如果差值大于等于5 手动跳帧 这里可根据自身需求来定
			this.videoElement.currentTime = this.videoElement.buffered.end(0); //手动跳帧
		}
		const { currentTime, buffered } = this.videoElement;
		const lastRangeIndex = buffered.length - 1;
		const start = buffered.start(lastRangeIndex);
        const end_tmp = buffered.end(lastRangeIndex);
		this._cleanupBufferedRanges(this.videoElement.currentTime, start, end_tmp);
	}

	//当缓冲区更新完成时触发： 基于指定当前时间 实现快进和快退的逻辑
    _onUpdateEndNew() {
        const { currentTime, buffered } = this.videoElement;
        if (buffered.length <= 0) return;
		this._onUpdateEndOld();
		/**
		 * 	lastRangeIndex: 缓存区中的最后一个时间片段
		 * 		浏览器会将已加载的视频缓冲区分割为多个时间片段
		 *	start: 最后一个段的起始时间
		 	end  : 最后一个段的结束时间
		 */
        const lastRangeIndex = buffered.length - 1;
        const start = buffered.start(lastRangeIndex);
        const end = buffered.end(lastRangeIndex);

		/**
		 * currentTime: 播放器当前播放的位置
		 * 	如下代码的含义：
		 * 		1. 当前时间在缓冲区间外：跳转到有效缓冲区起点
		 * 		2. 有进度变化且剩余缓冲充足： 预跳到缓冲末尾
		 */
        if (currentTime < start || currentTime > end) {
            this.videoElement.currentTime = start;
        } else if (currentTime - this.preVideoPlayPos !== 0 && end - currentTime > 1) {
            this.videoElement.currentTime = end;
        }
		this.preVideoPlayPos = currentTime;

        this._cleanupBufferedRanges(currentTime, start, end);
    }

	/**
	 *  sourceBuffer.updating:
	 * 		1. 是一个 布尔值 的只读属性
	 * 		2. 反映 SourceBuffer 当前是否正在执行异步操作：
	 * 		3. 如果 updating为 true, 不能操作 缓存区域
	 */
    _cleanupBufferedRanges(currentTime, start, end) {
        // 清理历史缓冲区（不包括最后一个时间段）
        for (let i = 0; i < this.videoElement.buffered.length - 1; i++) {
            const preStart = this.videoElement.buffered.start(i);
            const preEnd = this.videoElement.buffered.end(i);
            if (!this.sourceBuffer.updating) {
                this.sourceBuffer.remove(preStart, preEnd);
            }
        }

        // 当前时间距离起点>10秒：保留最近3秒(currentTime - 3)
        if (currentTime - start > 10 && !this.sourceBuffer.updating) {
            this.sourceBuffer.remove(0, currentTime - 3);
        }

		//剩余缓冲距离终点>10秒: 清除超出部分
        if (end - currentTime > 10 && !this.sourceBuffer.updating) {
            this.sourceBuffer.remove(0, end - 3);
        }
    }

	///////////////////////////////////////////////////////////////////////////////////
    _initWebSocket() {
        try {
            this.ws = new WebSocket(this.wsUrl);
			this.ws.binaryType = 'arraybuffer';

            this.ws.addEventListener("message", (e) => {
                if (!this.isSourceOpen) {
                    // console.error("Source is not opened.");
                    return;
                }

				if(!this.isFirstFrameParsed){
					let offset = 0;
					const buffer = e.data;
					const dataView = new DataView(buffer);
					this.parseMP4Box(dataView, offset);
				}
				// this.parseMP4Stream(e);

                this.streamDataQueue.push(e.data);
                this.processStreamDataQueue();
            });

            this.ws.addEventListener("open", () => console.log("WebSocket connection opened."));
            this.ws.addEventListener("close", () => console.log("WebSocket connection closed."));
            this.ws.addEventListener("error", (error) => console.error("WebSocket error:", error));
        } catch (error) {
            console.error("Failed to initialize WebSocket:", error.message);
            return { success: false, message: `Failed to initialize WebSocket: ${error.message}` };
        }
    }
	processStreamDataQueue() {
		//updating 为true时，不能操作缓冲区
        if (!this.sourceBuffer || this.sourceBuffer.updating || this.streamDataQueue.length === 0) return;

        if (this.streamDataQueue.length === 1) {
            this.sourceBuffer.appendBuffer(this.streamDataQueue.shift());
        } else {
            const totalLength = this.streamDataQueue.reduce((acc, streamData) => acc + streamData.byteLength, 0);
            const combinedBuffer = new Uint8Array(totalLength);

            let offset = 0;
            this.streamDataQueue.forEach(streamData => {
                combinedBuffer.set(new Uint8Array(streamData), offset);
                offset += streamData.byteLength;
            });
			this.streamDataQueue = [];
			this.sourceBuffer.appendBuffer(combinedBuffer);
        }
    }

	///////////////////////////////////////////////////////////////////////////////////
	parseMP4Box(dataView, offset = 0, parentBoxType = '') {
		while (offset < dataView.byteLength) {
			// 读取 Box 头部
			const boxSize = dataView.getUint32(offset);
			const boxType = new TextDecoder().decode(new Uint8Array(dataView.buffer, offset + 4, 4));
			// console.log(`Box Type: ${boxType}, Box Size: ${boxSize}, Parent Box: ${parentBoxType}`);

			// 处理 `tfdt` Box（包含 PTS 信息）
			if (boxType === 'tfdt') {
				let timestamp_len_desc = '32bit';
				const version = dataView.getUint8(offset + 8); // Box 版本
				if(version === 1){
					timestamp_len_desc = '64bit';
					this.firstFrameTimeStamp = dataView.getBigUint64(offset + 12);
				}else{
					this.firstFrameTimeStamp = dataView.getUint32(offset + 12);
				}
				console.log(`recv fist frame pts: ${this.firstFrameTimeStamp} ` + `box version is ${version}, so use ${timestamp_len_desc}`);
				this.isFirstFrameParsed = true;
				if(this.userCallbacks.onRecvedFirstFrame){
					// this.sourceBuffer.timestampOffset = -Math.floor(Number(this.firstFrameTimeStamp));
					this.userCallbacks.onRecvedFirstFrame(this.firstFrameTimeStamp);
				}
			}

			// 递归解析子 Box（`moof` 或 `traf`）
			if (boxType === 'moof' || boxType === 'traf') {
				this.parseMP4Box(dataView, offset + 8, boxType); // 递归解析子 Box
			}

			// 移动到下一个 Box
			offset += boxSize;
		}
	}

	parseMP4Stream(event) {
		const data = event.data;

        // 确保数据是 ArrayBuffer
        if (data instanceof ArrayBuffer) {
			data.fileStart = this.currentOffset;
            const nextOffset = this.mp4boxFile.appendBuffer(data);
			this.currentOffset = nextOffset;

            // 如果 nextOffset 为 0，表示文件已完全解析
            if (nextOffset === 0) {
                console.log('MP4 file fully parsed.');
            }
        } else {
            console.error('Received data is not an ArrayBuffer.');
        }
	}
	handleReady(info) {
        console.log('MP4 file is ready:', info);

		info.tracks.forEach((track, index) => {
			console.log(`Track ${index + 1}:`);
			console.log('  Type:', track.type); // 轨道类型（如 video, audio 等）
			console.log('  ID:', track.id); // 轨道 ID
			console.log('  Codec:', track.codec); // 编码器类型
			console.log('  Duration:', track.duration); // 轨道时长
			console.log('  Bitrate:', track.bitrate); // 比特率
			console.log('  Language:', track.language); // 语言（如果有）
			console.log('  Width:', track.video ? track.video.width : 'N/A'); // 视频宽度（如果是视频轨道）
			console.log('  Height:', track.video ? track.video.height : 'N/A'); // 视频高度（如果是视频轨道）
			console.log('--------------------------');

			this.mp4boxFile.setExtractionOptions(track.id, null, null);
			this.mp4boxFile.onSamples = this.handleSamples.bind(this);
			this.mp4boxFile.start();

		});
    }
	handleSamples(trackId, type, samples) {
		const track = this.mp4boxFile.getTrackById(trackId);
        if (!this.isFirstFrameParsed) {
            // 获取第一帧的 PTS
            const pts = samples[0].cts / this.mp4boxFile.getTrackById(trackId).timescale; // 转换为秒
            console.log('trackId:', trackId, 'type', track.type, 'First frame PTS:', pts, 'seconds', " cts:",  samples[0].cts, " timescale", this.mp4boxFile.getTrackById(trackId).timescale, " dts:", samples[0].dts);

            // 标记已解析第一帧
            this.isFirstFrameParsed = true;
        }
    }


}

export default VideoMSEPlayer;