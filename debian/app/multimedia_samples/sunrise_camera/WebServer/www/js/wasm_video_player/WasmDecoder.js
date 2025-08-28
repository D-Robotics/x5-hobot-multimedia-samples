// import { LiveStreamPullerRecv, LiveStreamPullerSent } from './DecoderCmd.js';
self.Module = {
	onRuntimeInitialized() {
		if (self.decoder) {
			self.decoder.onWasmLoaded();
		} else {
			console.log("[ER] No decoder!");
		}
	}
};
self.importScripts("RingBuffer.js");
self.importScripts("DecoderCmd.js");
self.importScripts("libffmpeg.js");


self.onmessage = function (evt) {

	if (!self.decoder) {
		console.log("[ER] Decoder not initialized!");
		return;
	}

	const req = evt.data;
	if (!self.decoder.wasmLoaded) {
		self.decoder.cacheReq(req);
		console.log(`Wasm not load, but cmd is comming, so cache it: cmd type is ${req.t}.`);
		return;
	}

	self.decoder.processReq(req);
};

class Decoder {
	constructor() {
		this.wasmLogLevel = 1;

		this.wasmLoaded = false;

		this.tmpReqQue = [];		//wasm load 完成前，post的命令先缓存起来

		this.wasmBuffer = null;		//从wasm中申请的内存
		this.wasmBufferSize = 1024 * 100; // 默认10K的大小


		this.decodeTimer = null;

		this.ringBufferMaxCount = 5;
		this.ringBuffer = null;
		this.videoCallback = null;
	}

	onWasmLoaded() {
		console.log("Wasm Decoder loaded.");
		this.wasmLoaded = true;

		//1. 解码后的视频帧，通过回调函数 返回给用户
		this.videoCallback = Module.addFunction((buff, size, timestamp) => {
			const outArray = Module.HEAPU8.subarray(buff, buff + size);
			const data = new Uint8Array(outArray);
			const objData = {
				t: DecoderSend.kVideoFrame,
				s: timestamp,
				d: data
			};
			self.postMessage(objData, [objData.d.buffer]);
		}, 'viid');

		//2. 处理 wasm load 之前的命令
		while (this.tmpReqQue.length > 0) {
			const req = this.tmpReqQue.shift();
			this.processReq(req);
		}
	}

	initDecoder() { //-1: 实时流
		this.ringBuffer = new NALUPointerRingBuffer(this.ringBufferMaxCount);
		this.wasmBuffer = Module._malloc(this.wasmBufferSize);
		const objData = {
			t: DecoderSend.kInitDecoderRsp,
			e: 0
		};
		self.postMessage(objData);
	}

	uninitDecoder() {

		Module._free(this.wasmBuffer);
		this.wasmBuffer = null;
		this.wasmBufferSize = 0;

		const objData = {
			t: DecoderSend.kUninitDecoderRsp,
			e: ret
		};
		self.postMessage(objData);
	}

	openDecoder(v) {
		const codecType = v.c === "h265" ? 1 : 0;
		const ret = Module._openDecoder(codecType, v.w, v.h, this.videoCallback, this.wasmLogLevel);
		const objData = {
			t: DecoderSend.kOpenDecoderRsp,
			e: ret
		};
		self.postMessage(objData);
	}

	closeDecoder() {
		if (this.decodeTimer) {
			clearInterval(this.decodeTimer);
			this.decodeTimer = null;
		}

		const ret = Module._closeDecoder();
		const objData = {
			t: DecoderSend.kCloseDecoderRsp,
			e: ret
		};
		self.postMessage(objData);
	}

	startDecoding(interval) {
		if (this.decodeTimer) {
			clearInterval(this.decodeTimer);
		}
		this.decodeTimer = setInterval(() => this.decode(), interval);
		const objData = {
			t: DecoderSend.kStartDecodingRsp,
			e: 0
		};
		self.postMessage(objData);
	}

	pauseDecoding() {
		if (this.decodeTimer) {
			clearInterval(this.decodeTimer);
			this.decodeTimer = null;
		}
		const objData = {
			t: DecoderSend.kPauseDecodingRsp,
			e: 0
		};
		self.postMessage(objData);
	}

	decode() {
		const bufferWithMeta = this.ringBuffer.pop();
		if (bufferWithMeta == null) {
			return;
		}
		const nalu = new Uint8Array(bufferWithMeta.data);

		//1. 动态扩大 wasm 内存
		const requiredSize = nalu.length;
		if (requiredSize > this.wasmBufferSize) {
			const newSize = Math.max(requiredSize, this.wasmBufferSize * 1.5);
			Module._free(this.wasmBuffer);
			this.wasmBuffer = Module._malloc(newSize);
			if (this.wasmBuffer === 0) throw new Error("malloc failed");
			console.warn(`wasm decoder grow to ${newSize}, require ${requiredSize}, current is ${this.wasmBufferSize}`);
			this.wasmBufferSize = newSize;
		}

		//2. 拷贝裸码率
		Module.HEAPU8.set(nalu, this.wasmBuffer);

		//3. 解码
		let ret = Module._decodeData(this.wasmBuffer, requiredSize, bufferWithMeta.timestamp);
		if (ret === 8) {
			if (this.decodeTimer) {
				console.error("stop wasm decoder.");
				clearInterval(this.decodeTimer);
				this.decodeTimer = null;
			}
			const objData = {
				t: DecoderSend.kDecodeFinished
			};
			self.postMessage(objData);
		}
	}

	sendData(data, timestamp) {

		const bufferWithMeta = {
			data: data,          // 原始数据
			timestamp: timestamp,   // 附加属性
			type: "video/hevc"        // 其他元数据
		};
		this.ringBuffer.push(bufferWithMeta);
	}

	cacheReq(req) {
		// 如果 req 不存在，直接返回
		if (!req) {
			return;
		}

		// 如果队列长度已经超过 100，不再缓存新请求
		if (this.tmpReqQue.length >= 100) {
			console.warn("[cacheReq] Temporary request queue is full (max 100), dropping new request");
			return;
		}

		// 否则，缓存请求
		this.tmpReqQue.push(req);
	}


	processReq(req) {
		switch (req.t) {
			case DecoderRecv.kInitDecoderReq:
				this.initDecoder();
				break;
			case DecoderRecv.kUninitDecoderReq:
				this.uninitDecoder();
				break;
			case DecoderRecv.kOpenDecoderReq:
				this.openDecoder(req.v);
				break;
			case DecoderRecv.kCloseDecoderReq:
				this.closeDecoder();
				break;
			case DecoderRecv.kStartDecodingReq:
				this.startDecoding(req.i);
				break;
			case DecoderRecv.kPauseDecodingReq:
				this.pauseDecoding();
				break;
			case DecoderRecv.kFeedDataReq:
				this.sendData(req.d, req.s);
				break;
			default:
				console.error("Unsupport messsage " + req.t + " req:" + req);
		}
	}
}

self.decoder = new Decoder();

