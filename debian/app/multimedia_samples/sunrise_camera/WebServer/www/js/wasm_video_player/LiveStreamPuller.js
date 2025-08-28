self.importScripts("mp4box.all.min.js");
self.importScripts("LiveStreamPullerCmd.js");

function getH265NALUTypeName(type) {
	const h265NALUTypes = {
		0: "Trail_N",
		1: "Trail_R",
		2: "TSA_N",
		3: "TSA_R",
		4: "STSA_N",
		5: "STSA_R",
		6: "RADL_N",
		7: "RADL_R",
		8: "RASL_N",
		9: "RASL_R",
		16: "BLA_W_LP",
		17: "BLA_W_RADL",
		18: "BLA_N_LP",
		19: "IDR_W_RADL",
		20: "IDR_N_LP",
		21: "CRA_NUT",
		32: "VPS",
		33: "SPS",
		34: "PPS",
		35: "AUD",
		36: "EOS",
		37: "EOB",
		38: "FD",
		39: "Prefix_SEI",
		40: "Suffix_SEI",
	};
	return h265NALUTypes[type] || "Unknown";
}

function avcToNALUInPlace(avcData, length) {

	const dataView = new DataView(avcData.buffer, avcData.byteOffset, avcData.byteLength);
	let pos = 0;
	while (pos + 4 <= avcData.length) {
		const naluSize = dataView.getUint32(pos);
		dataView.setUint32(pos, 0x00000001);
/*
		let naluType = (dataView.getUint8(pos + 4) >> 1) & 0x3F; // 取高 6 位
		console.log(`NALU Type: ${naluType} (${getH265NALUTypeName(naluType)})`);
*/
		pos += 4 + naluSize;
	}
	if((pos != avcData.byteLength) || (avcData.byteLength != length)){
		console.error(`[avc/hevc] to nalu failed: ${pos} != ${avcData.byteLength} !=  ${length}`);
	}
	return avcData;
}


class FMP4Parser {
	constructor() {
		this.mp4boxFile = MP4Box.createFile();
		this.videoTrackInfo = null;
		this.ready = false;
		this.currentStreamDataOffset = 0					// 视频流的偏移：mp4box 使用

		// 设置回调
		this.mp4boxFile.onReady = this._onReady.bind(this);
		this.mp4boxFile.onSamples = this._onSamples.bind(this);
		this.mp4boxFile.onError = this._onError.bind(this);
		this.mp4boxFile.onSegment = this._onSegment.bind(this);
	}

	parse(buffer, { shouldFlush = false } = {}) {
		if (!buffer || !(buffer instanceof ArrayBuffer)) {
			throw new Error("Invalid buffer provided");
		}

		if (!this.mp4boxFile) {
			throw new Error("MP4Box not initialized");
		}

		try {
			const workingBuffer = buffer.slice(0);

			//mp4box 要求必须有 fileStart 成员
			workingBuffer.fileStart = this.currentStreamDataOffset;
			const nextOffset = this.mp4boxFile.appendBuffer(workingBuffer);
			this.currentStreamDataOffset = nextOffset;

			if (shouldFlush) {  // 由调用方决定是否需要flush
				this.mp4boxFile.flush();
			}

			return nextOffset;
		} catch (error) {
			console.error("MP4Box parsing error:", error);
			this.currentStreamDataOffset = -1; // 标记错误状态
			throw error; // 抛出错误由上层处理
		}
	}

	_onReady(info) {
		if (info.videoTracks && info.videoTracks.length > 0) {
			this.videoTrackInfo = info.videoTracks[0];

			const { video, codec } = this.videoTrackInfo;
			const { width, height } = video;

			let format = "Unknown";
			if (codec.includes("avc1")) {
				format = "h264";
			} else if (codec.includes("hev1") || codec.includes("hvc1")) {
				format = "h265";
			} else if (codec.includes("av01")) {
				format = "av1";
			}
			console.log(`[MP4 Parser Ready] Video codec info: ${format} ${width}x${height}`);
			postVideoInfoToMain(width, height, format);

			this.mp4boxFile.setExtractionOptions(this.videoTrackInfo.id, null, {
				nbSamples: 100 // 限制提取的样本数（避免内存爆炸）
			});

			this.ready = true;
			this.mp4boxFile.start();
		} else {
			console.warn("No video track found in MP4 stream");
		}
	}


	_onSamples(trackId, ref, samples) {
		if (trackId === this.videoTrackInfo.id) {
			samples.forEach(sample => {
				const nalu_data = avcToNALUInPlace(sample.data, sample.size);
				postVideoDataToMain(sample.cts, nalu_data);
			});
		}
	}

	_onSegment(id, user, buffer, sampleNum) {
		// 可选:处理完整的分段
	}

	_onError(error) {
		console.error("MP4Box error:", error);
		postMessageToMain(LiveStreamPullerSent.Error, `MP4Box error: ${error.message}`);
	}
}

class LiveStreamPuller {
	constructor() {
		this.url = null;
		this.ws = null;
		this.parser = new FMP4Parser();
	}

	start(url) {
		this.url = url;

		try {
			if (!this.url) {
				console.error("URL is not set");
				return;
			}

			this.ws = new WebSocket(this.url);
			this.ws.onopen = () => {
				postMessageToMain(LiveStreamPullerSent.StartRsp);
			};

			this.ws.onmessage = async (message) => {
				try {
					let data = message.data;
					if (data instanceof Blob) {
						data = await data.arrayBuffer();
					}

					if (data instanceof ArrayBuffer) {
						this.parser.parse(data);
					}
				} catch (error) {
					console.error("Error processing WebSocket message:", error);
				}
			};

			this.ws.onerror = (error) => {
				postMessageToMain(LiveStreamPullerSent.Error, `WebSocket error: ${error.message}`);
			};

			this.ws.onclose = () => {
				postMessageToMain(LiveStreamPullerSent.StopRsp);
			};
		} catch (error) {
			console.error(`Error starting WebSocket: ${error.message}`);
			postMessageToMain(LiveStreamPullerSent.Error, `WebSocket init error: ${error.message}`);
		}
	}

	stop() {
		try {
			if (this.ws) {
				this.ws.close();
				this.ws = null;
			}
		} catch (error) {
			console.error(`Error stopping WebSocket: ${error.message}`);
			postMessageToMain(LiveStreamPullerSent.Error, `WebSocket stop error: ${error.message}`);
		}
	}
}

self.live_streame_puller = new LiveStreamPuller();

function postMessageToMain(type, message = {}) {
	try {
		const objData = {
			t: type,
			d: message
		};
		self.postMessage(objData);
	} catch (error) {
		console.error("Error in postMessageToMain:", error.message);
	}
}
function postVideoInfoToMain(width, height, codecType) {
	try {
		const objData = {
			t: LiveStreamPullerSent.FrameInfo,
			v: {
				w: width,
				h: height,
				c: codecType,
				p: "nv12"
			}
		};
		self.postMessage(objData);
	} catch (error) {
		console.error("Error in postVideoInfoToMain:", error.message);
	}
}

function postVideoDataToMain(dts, data) {
	try {
		if (data instanceof ArrayBuffer || ArrayBuffer.isView(data)) {
			const transferData = data instanceof ArrayBuffer ? data : data.buffer;
			self.postMessage({
				t: LiveStreamPullerSent.FrameData,
				s: dts,
				d: data
			}, [transferData]);
		} else {
			console.warn("Unsupported data type for transfer:", typeof data);
			postMessageToMain(type, data);
		}
	} catch (error) {
		console.error("Error in postVideoDataToMain:", error);
		postMessageToMain(LiveStreamPullerSent.Error, `Data posting error: ${error.message}`);
	}
}

self.addEventListener("message", (evt) => {
	try {
		const objData = evt.data;

		switch (objData.t) {
			case LiveStreamPullerRecv.StartReq:
				if (self.live_streame_puller) {
					self.live_streame_puller.start(objData.u);
				} else {
					console.error("Streamer start failed");
					postMessageToMain(LiveStreamPullerSent.Error, "Streamer initialization failed");
				}
				break;

			case LiveStreamPullerRecv.StopReq:
				if (self.live_streame_puller) {
					self.live_streame_puller.stop();
				}
				break;

			default:
				console.error("Unknown command", objData.t);
				postMessageToMain(LiveStreamPullerSent.Error, `Unknown command: ${objData.t}`);
		}
	} catch (error) {
		console.error(`Error processing message: ${error.message}`);
		postMessageToMain(LiveStreamPullerSent.Error, `Message processing error: ${error.message}`);
	}
});
