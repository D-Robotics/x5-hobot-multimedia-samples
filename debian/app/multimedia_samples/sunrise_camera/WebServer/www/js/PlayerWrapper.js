import VideoMSEPlayer from './VideoMSEPlayer.js'; // 导入 VideoMSEPlayer
import WasmVideoPlayer from './wasm_video_player/WasmVideoPlayer.js'

class PlayerWrapper {
    constructor(browserCapabilities, pipelineChannel, userCallbacks) {

		//业务相关参数
		this.pipelineChannel = pipelineChannel;
		this.browserCapabilities = browserCapabilities;
		this.userCallbacks = {
			onOpen: null,
			onRecvedFirstFrame: null,
			onFirstFrameLoaded: null,
			onStoped: null,
			onError: null
		};
		this.userCallbacks = userCallbacks;

		//播放器相关参数
		this.player = null;
        this.wsUrl = null;
		this.videoCodecType = null;
        this.videoElementId = null;
		this.attachVideoElementId = null;

		this.isStarted = false;
    }

	init(wsUrl, videoCodecType, getPlayerDisplayElementIdCb){
		this.videoCodecType = videoCodecType;
		this.wsUrl = wsUrl;

		const {videoPlayerType, displayElementType, attachDisplayElementType, mimeType} = this.chooseAppropriatePlayer(videoCodecType);
		this.videoPlayerType = videoPlayerType;
		this.displayElementType = displayElementType;
		this.mimeType = mimeType;

		this.videoElementId = getPlayerDisplayElementIdCb(this.pipelineChannel, displayElementType);

		if(attachDisplayElementType != null){
			this.attachVideoElementId = getPlayerDisplayElementIdCb(this.pipelineChannel, attachDisplayElementType);
		}
		console.log(`[${this.pipelineChannel}] player inited.
PlayerType:${this.videoPlayerType}, videoElementId:${this.videoElementId}, attachVideoElementId:${this.attachVideoElementId}`);
	}

    // 根据编码器类型初始化播放器
    start() {
		this.player = null;

        // 根据编码器类型选择播放器
        switch (this.videoPlayerType) {
            case 'MJPEGVideoPlayer':
                // player = new MjpegPlayer(this.wsUrl, videoElement);
                break;
            case 'MSEVideoPlayer':
                this.player = new VideoMSEPlayer(this.wsUrl, this.videoElementId, this.mimeType);
                break;
            case 'WasmVideoPlayer':
				this.player = new WasmVideoPlayer(this.wsUrl, this.videoElementId, this.mimeType);
                break;
            default:
                throw new Error('Unsupported codec type!');
        }
        // 初始化播放器
        if (this.player && typeof this.player.init === 'function') {
			const userCallbacks = {
				onOpen :this.onOpen.bind(this),
				onRecvedFirstFrame :this.onRecvedFirstFrame.bind(this),
				onFirstFrameLoaded :this.onFirstFrameLoaded.bind(this),
				onStoped :this.onStoped.bind(this),
				onError :this.onError.bind(this),
			}
            this.player.init(userCallbacks);
			if(this.attachVideoElementId != null){
				this.player.initAttachDisplayWindowInfo(this.attachVideoElementId);

				this.handleWindowResize = this._debounce(() => {
					if(this.player){
						this.player.adjustDisplayWindowSizeDynamically();
					}else{
						console.warn("player is destroyed.");
					}

				  }, 100);
				window.addEventListener('resize', this.handleWindowResize, { passive: true });
			}
        } else {
            throw new Error('Invalid player instance!');
        }

        // 启动播放器
        if (this.player && typeof this.player.start === 'function') {
            this.player.start();
			this.isStarted = true;
        } else {
            throw new Error('Invalid player instance!');
        }
        console.log(`Started player for codec: ${this.videoCodecType}`);
    }

    // 停止播放器
    stop() {
		if(!this.isStarted){
			return;
		}
        if (this.player && typeof this.player.stop === 'function') {
            this.player.stop();
        }else{
			console.warn("[player wrapper] not found stop function .", this.player);
		}
		if(this.attachVideoElementId != null){
			window.removeEventListener('resize', this.handleWindowResize);
		}
		this.isStarted = false;
		this.player = null;
		this.videoCodecType = null;
		this.attachVideoElementId = null;
    }
	currentTimeSecond(){
		return this.player.currentTimeSecond();
	}
	displayWindowSize(){
		return this.player.displayWindowSize();
	}
	VideoResolutionSize(){
		return this.player.VideoResolutionSize();
	}

	///////////////////////////////////////////////////////////////////////////////////////////
	_debounce(func, wait) {
		let timeout;
		return () => {
		  clearTimeout(timeout);
		  timeout = setTimeout(func, wait);
		};
	  }

	chooseAppropriatePlayer(videoCodecType){
		switch (videoCodecType.toLowerCase()) {
			case 'mjpeg':
                // player = new MjpegPlayer(this.wsUrl, videoElement);
                break;
            case 'h264':
				return {
					videoPlayerType: "MSEVideoPlayer",
					displayElementType: "video",
					attachDisplayElementType: null,
					mimeType:'avc1.64001e',

				};
            case 'h265':
				const {video, wasm, webgl} = this.browserCapabilities;
				if(video.hevc){
					return  {
						videoPlayerType: "MSEVideoPlayer",
						displayElementType: "video",
						attachDisplayElementType: null,
						mimeType: 'hvc1.1.6.L123.B0'
					};
				}else if(wasm.supported && (webgl.webgl1 ||webgl.webgl2)){
					return {
						videoPlayerType: "WasmVideoPlayer",
						displayElementType: "canvas",
						attachDisplayElementType: "video",
						mimeType: 'hvc1.1.6.L123.B0'
					};
				}else {
					return{
						videoPlayerType: "NotFoundVideoPlayer",
						attachDisplayElementType: null,
						displayElementType: null,
						mimeType: null
					};
				}
            default:
                throw new Error('Unsupported codec type!');
		}
	}
	/////////////////////////////////////////////////////////////////////////////////////////
	onOpen(){
		console.log('[', this.pipelineChannel, ']', ' is opend.');
		if(this.userCallbacks.onOpen){
			this.userCallbacks.onOpen(this.pipelineChannel);
		}

	}
	onRecvedFirstFrame(timestamp){
		console.log('[', this.pipelineChannel, ']', ' recv first frame, and timestamp is ', timestamp, '.');
		if(this.userCallbacks.onRecvedFirstFrame){
			this.userCallbacks.onRecvedFirstFrame(this.pipelineChannel, timestamp);
		}
	}
	onFirstFrameLoaded(){
		console.log('[', this.pipelineChannel, ']', ' is first frame loaded.');
		if(this.userCallbacks.onFirstFrameLoaded){
			this.userCallbacks.onFirstFrameLoaded(this.pipelineChannel);
		}
	}

	onStoped(){
		console.log('[', this.pipelineChannel, ']', ' is stoped.');
		if(this.userCallbacks.onStoped){
			this.userCallbacks.onStoped(this.pipelineChannel);
		}
	}
	onError(){
		console.log('[', this.pipelineChannel, ']', ' is error.');
		if(this.userCallbacks.onError){
			this.userCallbacks.onError(this.pipelineChannel);
		}
	}
}

export default PlayerWrapper;
