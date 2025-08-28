import ConfigManager from "./ConfigManager.js";
import DisplayWindowManager from "./DisplayWindowManager.js";
import WebSocketProtocolHandler from "./WebSocketProtocolHandler.js";

let app = null;
class App {
	constructor() {
		this.configManager = null;
		this.displayWindowManager = null;
		this.wsProtocolHandler = null;
		this.serverIp = null;
	}

	// 初始化应用
	init() {
		const host = window.location.host;     // 例如 "192.168.1.10:8080" 或 "example.com:8080"
		this.serverIp = host.split(':')[0];  // 提取 IP 地址或主机名

		this.configManager = new ConfigManager();
		this.displayWindowManager = new DisplayWindowManager();

		this.initWebSocketProtocolHandler();
		this.initDisplayWindowManager();
		this.initConfigManagerCallbacks();
	}

	// 初始化 WebSocket 连接
	initWebSocketProtocolHandler() {
		this.wsProtocolHandler = new WebSocketProtocolHandler();
		this.wsProtocolHandler.init(`ws://${this.serverIp}:4567`, {
			onopen: this.handleWebSocketOpen.bind(this),
			onclose: this.handleWebSocketClose.bind(this),
			onerror: this.handleWebSocketError.bind(this),
			onAppSwitch: this.handleAppSwitch.bind(this),
			onSnapshot: this.handleSnapshot.bind(this),
			onAlogResult: this.handleAlogResult.bind(this),
			onGetConfig: this.handleGetConfig.bind(this),
			onVideoFrameInfo: this.handleVideoFrameInfo.bind(this),
		});
	}

	// WebSocket 连接成功回调
	handleWebSocketOpen(event) {
		console.log("WebSocket 连接成功:", event);
		const currentTime = Date.now() / 1000;
		this.wsProtocolHandler.syncTime(currentTime);
		this.wsProtocolHandler.getConfig();
	}

	// WebSocket 关闭回调
	handleWebSocketClose(event) {
		console.log("WebSocket 连接关闭:", event);
	}

	// WebSocket 错误回调
	handleWebSocketError(event) {
		console.error("WebSocket 错误:", event);
	}

	// 处理 APP_SWITCH 命令
	handleAppSwitch(message) {
		console.log("收到 APP_SWITCH 命令:", message);
		if(message.Status == 200){
			this.configManager.buildHTMLFromConfig(true);
			this.startStream();
		}else{
			console.log("app switch is error:", message.app_status);
			this.showAppStatus(message.app_status, message.detailed);
			this.showErrorModal();

			if (message.solution_configs) {
				//确认按钮点击后，调用onResetButtionClicked：生效配置
				this.configManager.updateConfig(message.solution_configs);
			}else{
				console.log("app switch recv error data, no solution config.");
			}

		}
	}

	// 处理 SNAPSHOT 命令
	handleSnapshot(message) {
		console.log("收到 SNAPSHOT 命令:", message);
		this.downloadFile(message.Filename, this.serverIp);
	}

	// 处理 ALOG_RESULT 命令
	handleAlogResult(message) {
		this.displayWindowManager.pushAlogResult(message);
		// console.log("收到 ALOG_RESULT 命令:", message);
	}

	// 处理 GET_CONFIG 命令
	handleGetConfig(message) {
		console.log("GET_CONFIG 命令:", message);
		const { solution_configs } = message;

		if (!solution_configs) {
			console.error("GET_CONFIG cmd recv message don't have solution configs");
			return;
		}

		try {
			this.configManager.updateConfig(solution_configs);
			this.configManager.buildHTMLFromConfig(true);
			this.startStream();

		} catch (error) {
			console.error("处理 GET_CONFIG 命令时出错:", error);
		}
		console.log("GET_CONFIG 命令: 处理完成");
	}
	handleVideoFrameInfo(message){
		this.displayWindowManager.updateVideoFrameInfo(message);
	}
	// 初始化显示窗口的回调函数
	initDisplayWindowManager() {
		const callbacks = {
			onCaptureVIN: (video_index) => this.handleCapture('vin', 'raw', video_index),
			onCaptureISP: (video_index) => this.handleCapture('isp', 'yuv', video_index),
			onCaptureVSE: (video_index) => this.handleCapture('vse', 'yuv', video_index),
		};
		this.displayWindowManager.init(this.serverIp, callbacks);
	}

	// 处理捕获命令
	handleCapture(type, format, video_index) {
		const cmdData = { type, format, videoNum: video_index };
		this.wsProtocolHandler.snapshot(cmdData);
	}

	// 初始化配置管理器的回调函数
	initConfigManagerCallbacks() {
		const callbacks = {
			onChange: (stream_count, isFourceUpdateWindow) => {
				this.displayWindowManager.updateLayout(stream_count, isFourceUpdateWindow);
			},
		};
		this.configManager.init(callbacks);
	}

	startStream() {
		console.log("start stream .");
		let codec_types = this.configManager.getStreamCodecType();
		this.displayWindowManager.startPlayer(codec_types);
		let display_window_count = this.displayWindowManager.getDisplayWindowCount();
		this.wsProtocolHandler.startStream(Number(display_window_count));
	}

	stopStream() {
		console.log("stop stream .");
		this.displayWindowManager.stopPlayer();
		let display_window_count = this.displayWindowManager.getDisplayWindowCount();
		this.wsProtocolHandler.stopStream(Number(display_window_count));
	}
	stopPlayer(){
		console.log("stop player .");
		this.displayWindowManager.stopPlayer();
	}
	startPlayer() {
		console.log("start player .");
		let codec_types = this.configManager.getStreamCodecType();
		this.displayWindowManager.startPlayer(codec_types);
	}

	downloadFile(filePath, serverIp) {
		try {
		  const fileName = filePath.substring(filePath.lastIndexOf('/') + 1);
		  const encodedFileName = encodeURIComponent(fileName);
		  const baseUrl = `http://${serverIp}/tmp_file/`
		  const fileUrl = `${baseUrl}${encodedFileName}`;
		  console.log('下载文件:', fileUrl);

		  const downloadLink = document.createElement('a');
		  downloadLink.href = fileUrl;
		  downloadLink.download = fileName;
		  downloadLink.style.display = 'none';
		  document.body.appendChild(downloadLink);
		  downloadLink.click();
		  document.body.removeChild(downloadLink);
		} catch (error) {
		  console.error('下载文件失败:', error);
		}
	  }
	showAppStatus(message, detailed) {
		const errorText = document.getElementById("errorText");
		 // 如果 detailed 存在且是数组，处理数组内容
		 if (detailed && Array.isArray(detailed)) {
			// 将数组中的字符串用换行符连接
			const detailedString = detailed.join('\n');
			// 将 detailed 内容追加到 message 之后，并换行
			message += '\n' + detailedString.replace(/"/g, '');
		}
		// 更新 errorText 的内容
		errorText.textContent = message;
	}
	showErrorModal() {
		document.getElementById("errorModal").style.display = "flex";
	}
	hideErrorModal() {
		document.getElementById("errorModal").style.display = "none";
	}
}

// 页面加载完成后初始化应用
$(document).ready(() => {
	console.log("init application.");
	app = new App();
	app.init();
	app.hideErrorModal();
});

document.addEventListener("visibilitychange", () => {
	console.log("visibilitychange: " + document.hidden);
	if (document.hidden) {
		app.stopStream();
	} else {
		app.startStream();
	}
});

// 绑定按钮点击事件
window.onToggleVisibilityButtonClicked = () => {
	app.configManager.onToggleVisibility();
};

window.onSwitchSolutionButtonClicked = () => {
	app.stopStream();
	app.configManager.updateConfigFromHTML();
	const serverConfig = app.configManager.getConfigWithJson();
	app.wsProtocolHandler.appSwitch(serverConfig);
};

window.onSaveSolutionButtionClicked = () => {
	const serverConfig = app.configManager.getConfigWithJson();
	app.wsProtocolHandler.saveConfigs(serverConfig);
};

window.onRecoverySolutionButtonClicked = () => {
	app.wsProtocolHandler.recoveryConfigs();
};

//错误消息提示框的确认按钮
window.onResetButtionClicked = () => {
	app.hideErrorModal();
	app.configManager.buildHTMLFromConfig(true);
	app.startStream();
};
