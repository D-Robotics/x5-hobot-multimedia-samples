class WebSocketClient {
    constructor() {
        if (!WebSocketClient.instance) {
            this.websock = null;
            this.ws_url = "ws://192.168.1.10:4567";
            this.socket_open = false;
            this.hearbeat_timer = null;
            this.hearbeat_interval = 5000;

            this.is_reconnect = false;
            this.reconnect_count = 3;
            this.reconnect_current = 1;
            this.reconnect_timer = null;
            this.reconnect_interval = 3000;

            // 用户自定义回调
            this.userCallbacks = {
                onopen: null,
                onmessage: null,
                onclose: null,
                onerror: null
            };

            WebSocketClient.instance = this;
        }
        return WebSocketClient.instance;
    }

	log(message, level = 'info') {
		if (this.logLevel === 'debug' || level === 'error') {
			console[level](message);
		}
	}


    /**
     * 初始化 WebSocket 连接
     * @param {string} url WebSocket 服务器地址
     * @param {Object} callbacks 用户自定义回调函数
     * @returns {Object} { success: boolean, message?: string, error?: string }
     */
    init(url = this.ws_url, callbacks = {}) {
		if (this.websock && this.websock.readyState === WebSocket.OPEN) {
			console.log("WebSocket 连接已存在，不重复创建");
			return { success: false, error: "WebSocket connection already exists" };
		}

        if (!("WebSocket" in window)) {
            console.error("浏览器不支持 WebSocket");
            return { success: false, error: "WebSocket not supported by the browser" };
        }

        if (this.websock) {
            console.log("WebSocket 连接已存在，不重复创建");
            return { success: false, error: "WebSocket connection already exists" };
        }

        this.ws_url = url;
        this.userCallbacks = { ...this.userCallbacks, ...callbacks };

        try {
            this.websock = new WebSocket(this.ws_url);
            this.websock.binaryType = "arraybuffer";

            this.websock.onopen = (event) => this.onOpen(event);
            this.websock.onmessage = (event) => this.onMessage(event);
            this.websock.onclose = (event) => this.onClose(event);
            this.websock.onerror = (error) => this.onError(error);

            return { success: true, message: "WebSocket connection initiated" };
        } catch (error) {
            console.error("WebSocket 连接失败:", error);
            return { success: false, error: error.message };
        }
    }

    /**
     * WebSocket 连接成功回调
     */
    onOpen(event) {
        console.log("WebSocket 连接成功");
        this.socket_open = true;
        this.is_reconnect = true;

        if (this.userCallbacks.onopen) {
            this.userCallbacks.onopen(event);
        }
    }

    /**
     * WebSocket 消息接收回调
     */
    onMessage(event) {
        // console.log("WebSocket 收到消息:", event.data);
		if (event.data instanceof Blob || event.data instanceof ArrayBuffer) {
			// 处理二进制数据
			// console.log("WebSocket 收到二进制数据:", event.data);
		} else {
			// console.log("WebSocket 收到消息:", event.data);
		}
        if (this.userCallbacks.onmessage) {
            this.userCallbacks.onmessage(event);
        }
    }

    /**
     * WebSocket 关闭回调
     */
    onClose(event) {
        console.log("WebSocket 连接关闭:", event.code);
		if (event.wasClean) {
			console.log("WebSocket 连接正常关闭:", event.code);
		} else {
			console.error("WebSocket 连接异常关闭:", event.code);
		}
        this.socket_open = false;
        this.websock = null;
        clearInterval(this.hearbeat_timer);

        if (this.userCallbacks.onclose) {
            this.userCallbacks.onclose(event);
        }

        if (this.is_reconnect) {
            this.reconnect();
        }
    }

    /**
     * WebSocket 连接错误回调
     */
    onError(error) {
        console.error("WebSocket 发生错误:", error);

        if (this.userCallbacks.onerror) {
            this.userCallbacks.onerror(error);
        }
    }

    /**
     * 发送消息
     * @param {Object} data 要发送的数据
     * @returns {Object} { success: boolean, error?: string }
     */
    send(data) {
        if (!this.websock || this.websock.readyState !== WebSocket.OPEN) {
            console.error("WebSocket 未连接，消息发送失败:", data);
            return { success: false, error: "WebSocket not connected" };
        }

        try {
            this.websock.send(JSON.stringify(data));
            return { success: true };
        } catch (error) {
            console.error("WebSocket 发送消息失败:", error);
            return { success: false, error: error.message };
        }
    }

    /**
     * 开启心跳
     */
    startHeartbeat() {
        console.log("启动 WebSocket 心跳机制");
        clearInterval(this.hearbeat_timer);
        this.hearbeat_timer = setInterval(() => {
            const data = { kind: 0, "API-Source": "MERCHANT" };
            this.send(data);
        }, this.hearbeat_interval);
    }

    /**
     * 关闭 WebSocket 连接
     */
	close() {
		console.log("手动关闭 WebSocket 连接");
		clearInterval(this.hearbeat_timer);
		clearTimeout(this.reconnect_timer);
		this.is_reconnect = false;
		if (this.websock) {
			this.websock.onopen = null;
			this.websock.onmessage = null;
			this.websock.onclose = null;
			this.websock.onerror = null;
			this.websock.close();
			this.websock = null;
		}
	}


    /**
     * 重新连接 WebSocket
     */
	reconnect() {
		if (this.reconnect_current > this.reconnect_count) {
			console.warn("达到最大重连次数，停止重连");
			return;
		}
		this.reconnect_current++;
		const delay = Math.pow(2, this.reconnect_current) * 1000; // 指数退避
		console.log(`尝试重连 WebSocket (${this.reconnect_current}/${this.reconnect_count})，等待 ${delay}ms`);
		setTimeout(() => this.init(this.ws_url, this.userCallbacks), delay);
	}
	/**
	 * 获取 WebSocket 连接状态
	 * @returns {string} "connecting" | "open" | "closing" | "closed"
	 */
	getConnectionState() {
		if (this.websock) {
			switch (this.websock.readyState) {
				case WebSocket.CONNECTING: return "connecting";
				case WebSocket.OPEN: return "open";
				case WebSocket.CLOSING: return "closing";
				case WebSocket.CLOSED: return "closed";
				default: return "unknown";
			}
		}
		return "closed";
	}


}

export default WebSocketClient;
