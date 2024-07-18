// 导出socket对象
// export default class {
//   socket
// }

// socket主要对象
var socket = {
	websock: null,
	// 固定的WebSocket地址：设置默认的地址
	ws_url: "ws://192.168.1.10:4567",
	// 开启标识
	socket_open: false,
	// 心跳timer
	hearbeat_timer: null,
	// 心跳发送频率
	hearbeat_interval: 5000,

	// 是否自动重连
	is_reonnect: false,
	// 重连次数
	reconnect_count: 3,
	// 已发起重连次数
	reconnect_current: 1,
	// 重连timer
	reconnect_timer: null,
	// 重连频率
	reconnect_interval: 3000,

	// Wfs 句柄
	wfs_handle: new Array(64).fill(null),
	// 接收码流的帧率
	play_fps: new Array(64).fill(null),
	// 接收智能结果的帧率
	smart_fps: new Array(64).fill(null),
	// 记录接收到的第一帧码流的时间戳
	stream_first_timestamp: new Array(64).fill(BigInt(-1)),

	/**
	 * 初始化连接
	 */
	init: function (url) {
		if (!("WebSocket" in window)) {
			console.log('浏览器不支持WebSocket')
			return null
		}

		// 已经创建过连接不再重复创建
		if (socket.websock) {
			console.log("已经创建过连接不再重复创建");
			return socket.websock
		}

		socket.ws_url = url
		socket.websock = new WebSocket(socket.ws_url)
		socket.websock.binaryType = 'arraybuffer';
		socket.websock.onmessage = function (e) {
			socket.receive(e)
		}

		// 关闭连接
		socket.websock.onclose = function (e) {
			console.log('连接已断开')
			console.log('connection closed (' + e.code + ')')
			clearInterval(socket.hearbeat_interval)
			socket.socket_open = false
			socket.websock = null

			// 断开链接的时候就要销毁解码器
			// 遍历 wfs_handle 数组并销毁其中的对象
			for (var i = 0; i < socket.wfs_handle.length; i++) {
				var wfs = socket.wfs_handle[i];
				if (wfs) {
					wfs.destroy();
				}
			}

			// 需要重新连接
			if (socket.is_reonnect) {
				socket.reconnect_timer = setTimeout(function () {
					// 超过重连次数
					if (socket.reconnect_current > socket.reconnect_count) {
						clearTimeout(socket.reconnect_timer)
						return
					}

					// 记录重连次数
					socket.reconnect_current++
					socket.reconnect()
				}, socket.reconnect_interval)
			}
		}

		// 连接成功
		socket.websock.onopen = function () {
			console.log('连接成功')
			socket.socket_open = true
			socket.is_reonnect = true
			// 开启心跳
			socket.heartbeat()

			ws_onopen();

		}

		// 连接发生错误
		socket.websock.onerror = function () {
			console.log('WebSocket连接发生错误')
		}
	},
	/**
	 * 发送消息
	 * @param {*} data 发送数据
	 * @param {*} callback 发送后的自定义回调函数
	 */
	send: function (data, callback) {
		// 判断当前状态，如果已断连，则重新连接
		/*console.log("socket.socket_open: " + socket.socket_open);*/
		// 开启状态直接发送
		if (socket.websock.readyState === socket.websock.OPEN) {
			socket.websock.send(JSON.stringify(data))

			if (callback) {
				callback()
			}

			// 正在开启状态，则等待200ms后重新调用
		} else if (socket.websock.readyState === socket.websock.CONNECTING) {
			setTimeout(function () {
				socket.send(data, callback)
			}, 200)

			// 未开启，则等待200ms后重新调用
		} else {
			socket.init(socket.url)
			setTimeout(function () {
				socket.send(data, callback)
			}, 200)
		}
	},

	/**
	 * 接收消息
	 * @param {*} message 接收到的消息
	 */
	receive: function (message) {
		// console.log("data type:" + typeof message.data)
		// console.log('收到服务器内容：', message.data);

		if (typeof message.data === "string") {
			var params = JSON.parse(message.data)

			if (params == undefined) {
				console.log("收到服务器空内容");
				return false;
			}

			// 以下是接收消息后的业务处理，仅供参考

			// 被服务器强制断开
			if (params.kind != undefined && params.kind == 110) {
				socket.socket_open = false
				socket.is_reonnect = true
				// 被服务器踢掉
			} else if (params.kind == 99) {
				socket.socket_open = true
				socket.is_reonnect = false
				console.log("被挤下线 不做处理")
				return false
			} else {
				handle_ws_recv(params);
			}
		} else { // ArrayBuffer Blob
			var copy = new Uint8Array(message.data);
			// console.log("收到编码数据: " + copy.length + " copy: " + copy.slice(0, 32))

			// 获取0-3字节
			var chn_id = 0;
			for (var i = 0; i < 4; i++) {
				chn_id |= copy[i] << (8 * i);
			}

			// 获取4-11字节
			var timestamp = BigInt(0); // 将 timestamp 声明为 64 位整数
			for (var i = 4; i < 12; i++) {
				timestamp |= BigInt(copy[i]) << BigInt(8 * (i - 4)); // 使用 BigInt 对象进行位运算
			}
			// 设置时间戳
			if (socket.stream_first_timestamp[chn_id] == -1) {
				socket.stream_first_timestamp[chn_id] = timestamp;
				console.log("通道id:", chn_id, " 重置码流时间戳:",
					timestamp.toString());
			}

			// 发送给wfs，封装成MP4给 web的 video 播放
			socket.wfs_handle[chn_id].feedH264RawDate(copy.subarray(4));

			// 这里收到的都是H264编码的nalu数据，包含 1 5 7 8 四种类型， 1 5 是实际数据，所以统计帧率只统计 1 5
			// 7 帧长度一般 25 ，8 帧长度 33，所以只要大于33 的帧都认为是 1 5 数据帧，进行帧率统计
			if (copy.length - 12 > 33)
				socket.play_fps[chn_id]++;
		}
	},

	/**
	 * 心跳
	 */
	heartbeat: function () {
		console.log('socket', 'ping')
		if (socket.hearbeat_timer) {
			clearInterval(socket.hearbeat_timer)
		}

		socket.hearbeat_timer = setInterval(function () {
			var data = {
				kind: 0, //请求类型 kind 0 心跳包
				'API-Source': 'MERCHANT',
			}
			socket.send(data)
		}, socket.hearbeat_interval)
	},

	/**
	 * 主动关闭连接
	 */
	close: function () {
		console.log('主动断开连接')
		clearInterval(socket.hearbeat_interval)
		socket.is_reonnect = false
		socket.websock.close()
	},

	/**
	 * 重新连接
	 */
	reconnect: function () {
		if (socket.websock && socket.socket_open) {
			socket.websock.close()
		}

		socket.init(socket.ws_url)
	},
}
