import WebSocketClient from "./WebSocketClient.js";

class WebSocketProtocolHandler {
    constructor() {
        if (WebSocketProtocolHandler.instance) {
            return WebSocketProtocolHandler.instance;
        }
        WebSocketProtocolHandler.instance = this;

        // 命令类型常量（内部使用）
        this.REQUEST_TYPES = {
            APP_SWITCH: 1,
            SNAPSHOT: 2,
            START_STREAM: 3,
            STOP_STREAM: 4,
            SYNC_TIME: 5,
            SET_BITRATE: 6,
            GET_CONFIG: 7,
            SAVE_CONFIGS: 8,
            RECOVERY_CONFIGS: 9,
            ALOG_RESULT: 10,
			VIDEO_FRAME_INFO: 11,
        };

        this.websocketClient = new WebSocketClient(); // 单例 WebSocketClient
        this.userCallbacks = {}; // 存储用户定义的回调函数
    }

    /**
     * 初始化 WebSocket 连接
     * @param {string} url WebSocket 服务器地址
     * @param {Object} callbacks 用户定义的回调函数
     */
    init(url, callbacks = {}) {
        this.userCallbacks = callbacks;
        this.websocketClient.init(url, {
            onmessage: (event) => this.handleMessage(event),
            onopen: (event) => {
				this.handleOpenEvent();
				if (this.userCallbacks.onopen) {
                    this.userCallbacks.onopen(event);
                }
            },
            onclose: (event) => {
                if (this.userCallbacks.onclose) {
                    this.userCallbacks.onclose(event);
                }
            },
            onerror: (event) => {
                if (this.userCallbacks.onerror) {
                    this.userCallbacks.onerror(event);
                }
            },
        });
    }


    /**
     * 构建命令对象
     * @param {number} kind 命令类型
     * @param {Object} data 命令参数
     * @returns {Object} 命令对象
     */
    buildCommand(kind, data = {}) {
        return { kind, param: data };
    }

    /**
     * 发送命令
     * @param {number} kind 命令类型
     * @param {Object} data 命令参数
     * @returns {Object} { success: boolean, error?: string }
     */
    sendCommand(kind, data = {}) {
        const cmd = this.buildCommand(kind, data);
        return this.websocketClient.send(cmd);
    }

	handleOpenEvent(){
		this.websocketClient.startHeartbeat();
	}
    /**
     * 处理接收到的消息
     * @param {Event} event WebSocket 消息事件
     */
    handleMessage(event) {
		if (event.data instanceof Blob || event.data instanceof ArrayBuffer) {
			// 处理二进制数据
			// console.log("WebSocket ignore bin");
			return;
		}

        try {
            const message = JSON.parse(event.data);
            if (message && message.kind) {
                // console.log(`收到消息: kind=${message.kind}`, message);

                // 解析命令类型并调用对应的回调函数
                switch (message.kind) {
                    case this.REQUEST_TYPES.APP_SWITCH:
                        if (this.userCallbacks.onAppSwitch) {
                            this.userCallbacks.onAppSwitch(message);
                        }
                        break;
                    case this.REQUEST_TYPES.SNAPSHOT:
                        if (this.userCallbacks.onSnapshot) {
                            this.userCallbacks.onSnapshot(message);
                        }
                        break;
                    case this.REQUEST_TYPES.ALOG_RESULT:
                        if (this.userCallbacks.onAlogResult) {
                            this.userCallbacks.onAlogResult(message);
                        }
                        break;
                    case this.REQUEST_TYPES.GET_CONFIG:
                        if (this.userCallbacks.onGetConfig) {
                            this.userCallbacks.onGetConfig(message);
                        }
                        break;
					case this.REQUEST_TYPES.VIDEO_FRAME_INFO:
						if (this.userCallbacks.onVideoFrameInfo) {
                            this.userCallbacks.onVideoFrameInfo(message);
                        }
						break;
                    default:
                        console.warn(`未知的命令类型: kind=${message.kind}`);
                }
            }
        } catch (error) {
            console.error("消息解析失败:", error);
        }
    }

    // 封装支持的命令函数（对外暴露的方法）
    syncTime(currentTime) {
        return this.sendCommand(this.REQUEST_TYPES.SYNC_TIME, currentTime);
    }

    getConfig() {
        return this.sendCommand(this.REQUEST_TYPES.GET_CONFIG);
    }

    appSwitch(serverConfig) {
        return this.sendCommand(this.REQUEST_TYPES.APP_SWITCH, serverConfig);
    }

    saveConfigs(serverConfig) {
        return this.sendCommand(this.REQUEST_TYPES.SAVE_CONFIGS, serverConfig);
    }

    recoveryConfigs() {
        return this.sendCommand(this.REQUEST_TYPES.RECOVERY_CONFIGS);
    }

    startStream(display_window_count) {
        return this.sendCommand(this.REQUEST_TYPES.START_STREAM, display_window_count);
    }

    stopStream(display_window_count) {
        return this.sendCommand(this.REQUEST_TYPES.STOP_STREAM, display_window_count);
    }

    snapshot(data) {
        return this.sendCommand(this.REQUEST_TYPES.SNAPSHOT, data);
    }
}

export default WebSocketProtocolHandler;