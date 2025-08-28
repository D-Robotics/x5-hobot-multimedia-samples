class BrowserCapabilityDetector {
	constructor() {
		this.mediaSource = window.MediaSource || window.WebKitMediaSource;

		// 视频编解码器配置
		this.videoCodecs = [
			{ name: 'HEVC (hvc1)', mime: 'video/mp4; codecs="hvc1.1.6.L93.B0"', type: 'hevc' },
			{ name: 'HEVC (hev1)', mime: 'video/mp4; codecs="hev1.1.6.L93.B0"', type: 'hevc' },
			{ name: 'H.264 (Baseline)', mime: 'video/mp4; codecs="avc1.42E01E"', type: 'h264' },
			{ name: 'H.264 (Main)', mime: 'video/mp4; codecs="avc1.4D401E"', type: 'h264' },
			{ name: 'H.264 (High)', mime: 'video/mp4; codecs="avc1.64001E"', type: 'h264' }
		];
	}

	// 检测视频编解码支持
	detectVideoSupport() {
		const results = {
			hevc: false,
			h264: false,
			supportedFormats: []
		};

		if (!this.mediaSource) {
			console.warn('MediaSource API不可用，无法检测视频编解码支持');
			return results;
		}

		this.videoCodecs.forEach(codec => {
			try {
				const isSupported = this.mediaSource.isTypeSupported(codec.mime);
				if (isSupported) {
					if (codec.type === 'hevc') results.hevc = true;
					if (codec.type === 'h264') results.h264 = true;
					results.supportedFormats.push({
						name: codec.name,
						mime: codec.mime,
						type: codec.type
					});
				}
			} catch (e) {
				console.error('检测编解码器支持时出错:', e);
			}
		});
		return results;
	}

	// 检测WebAssembly支持
	detectWASMSupport() {
		const result = {
			supported: false,
			error: null,
			details: {}
		};

		try {
			if (typeof WebAssembly === 'object' && typeof WebAssembly.instantiate === 'function') {
				result.supported = true;
				result.details = {
					streaming: typeof WebAssembly.compileStreaming === 'function',
					simd: this._checkWASMSIMD(),
					threads: this._checkWASMThreads(),
					exceptionHandling: this._checkWASMExceptions()
				};
			} else {
				result.error = 'WebAssembly对象不可用';
			}
		} catch (err) {
			result.error = err.message;
		}
		return result;
	}

	// 检测WASM SIMD支持
	_checkWASMSIMD() {
		try {
			// 检查SIMD的WebAssembly验证码
			return WebAssembly.validate(new Uint8Array([
				0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 123, 3, 2, 1, 0, 10, 10, 1, 8, 0, 65, 0, 253, 15, 253, 98, 11
			]));
		} catch (e) {
			return false;
		}
	}

	// 检测WASM线程支持
	_checkWASMThreads() {
		try {
			// 检查线程的WebAssembly验证码
			return WebAssembly.validate(new Uint8Array([
				0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 3, 2, 1, 0, 6, 6, 1, 127, 1, 65, 0, 11, 10, 4, 1, 2, 0, 11
			]));
		} catch (e) {
			return false;
		}
	}

	// 检测WASM异常处理支持
	_checkWASMExceptions() {
		try {
			// 检查异常处理的WebAssembly验证码
			return WebAssembly.validate(new Uint8Array([
				0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 3, 2, 1, 0, 5, 4, 1, 1, 1, 1, 8, 1, 1, 10, 8, 1, 6, 0, 1, 65, 0, 26, 11
			]));
		} catch (e) {
			return false;
		}
	}

	/**
	 * 检测WebGL支持情况
	 */
	detectWebGLSupport() {
		const result = {
			webgl1: false,
			webgl2: false,
			renderer: null,
			vendor: null,
			maxTextureSize: null,
			maxParameters: {},
			extensions: []
		};

		try {
			// 检测WebGL 1.0
			const canvas = document.createElement('canvas');
			const gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');

			if (gl) {
				result.webgl1 = true;
				result.renderer = gl.getParameter(gl.RENDERER);
				result.vendor = gl.getParameter(gl.VENDOR);
				result.maxTextureSize = gl.getParameter(gl.MAX_TEXTURE_SIZE);

				// 记录一些重要参数
				result.maxParameters = {
					MAX_VERTEX_ATTRIBS: gl.getParameter(gl.MAX_VERTEX_ATTRIBS),
					MAX_VERTEX_UNIFORM_VECTORS: gl.getParameter(gl.MAX_VERTEX_UNIFORM_VECTORS),
					MAX_FRAGMENT_UNIFORM_VECTORS: gl.getParameter(gl.MAX_FRAGMENT_UNIFORM_VECTORS),
					MAX_TEXTURE_IMAGE_UNITS: gl.getParameter(gl.MAX_TEXTURE_IMAGE_UNITS),
				};

				// 获取支持的扩展
				const extList = gl.getSupportedExtensions();
				if (extList && extList.length > 0) {
					result.extensions = extList;
				}
			}

			// 检测WebGL 2.0
			const gl2 = canvas.getContext('webgl2');
			if (gl2) {
				result.webgl2 = true;
			}
		} catch (e) {
			console.error('检测WebGL支持时出错:', e);
		}
		return result;
	}

	// 输出视频检测结果
	_logVideoResults(results) {
		console.group('视频编解码支持检测');

		if (results.supportedFormats.length > 0) {
			console.log('%c✅ 支持的编解码: %s%s',
				'color: green',
				results.hevc ? 'HEVC ' : '',
				results.h264 ? 'H.264' : ''
			);

			console.table(results.supportedFormats.map(format => ({
				'编解码类型': format.type.toUpperCase(),
				'格式名称': format.name,
				'MIME类型': format.mime,
				'状态': '✔️ 支持'
			})));
		} else {
			console.log('%c❌ 未检测到支持的视频编解码', 'color: red');
		}

		console.groupEnd();
	}

	// 输出WASM检测结果
	_logWASMResult(result) {
		console.group('WebAssembly支持检测');

		if (result.supported) {
			console.log('%c✅ WebAssembly基本支持', 'color: green');
			console.log('高级功能支持:');
			console.table(result.details);
		} else {
			console.log('%c❌ WebAssembly不支持: %s',
				'color: red',
				result.error || '未知原因'
			);
		}

		console.groupEnd();
	}

	// 输出WebGL检测结果
	_logWebGLResult(result) {
		console.group('WebGL支持检测');

		if (result.webgl1 || result.webgl2) {
			const version = result.webgl2 ? '2.0' : (result.webgl1 ? '1.0' : '不支持');
			console.log(`%c✅ WebGL ${version} 支持`, 'color: green');

			console.log('渲染器信息:');
			console.table({
				'渲染器': result.renderer,
				'供应商': result.vendor,
				'最大纹理尺寸': result.maxTextureSize
			});

			// console.log('重要参数:');
			// console.table(result.maxParameters);

			// if (result.extensions.length > 0) {
			// 	console.log(`支持的扩展 (${result.extensions.length}个):`);
			// 	console.table(result.extensions.map(ext => ({ '扩展名称': ext })));
			// } else {
			// 	console.log('未检测到支持的扩展');
			// }
		} else {
			console.log('%c❌ 未检测到WebGL支持', 'color: red');
		}

		console.groupEnd();
	}
	showAll(browserCapabilities) {
		const {video, wasm, webgl} = browserCapabilities;

		this._logVideoResults(video);
		this._logWASMResult(wasm);
		this._logWebGLResult(webgl);

		return { video, wasm, webgl };
	}

	detectAll() {
		return {
			video: this.detectVideoSupport(),
			wasm: this.detectWASMSupport(),
			webgl: this.detectWebGLSupport()
		};
	}
}
export default BrowserCapabilityDetector;


