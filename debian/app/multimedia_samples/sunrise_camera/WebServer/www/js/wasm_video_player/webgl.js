function Texture(gl) {
	this.gl = gl;
	this.texture = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, this.texture);

	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);

	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
}

Texture.prototype.bind = function (n, program, name) {
	var gl = this.gl;
	gl.activeTexture([gl.TEXTURE0, gl.TEXTURE1, gl.TEXTURE2][n]);
	gl.bindTexture(gl.TEXTURE_2D, this.texture);
	gl.uniform1i(gl.getUniformLocation(program, name), n);
};

Texture.prototype.fill = function (width, height, data) {
	var gl = this.gl;
	gl.bindTexture(gl.TEXTURE_2D, this.texture);
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.LUMINANCE, width, height, 0, gl.LUMINANCE, gl.UNSIGNED_BYTE, data);
};

function WebGLPlayer(canvas, options) {
	this.canvas = canvas;
	this.gl = canvas.getContext("webgl") || canvas.getContext("experimental-webgl");
	this.initGL(options);
}

WebGLPlayer.prototype.initGL = function (options) {
	if (!this.gl) {
		console.log("[ER] WebGL not supported.");
		return;
	}

	var gl = this.gl;
	gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
	var program = gl.createProgram();
	var vertexShaderSource = [
		"attribute highp vec4 aVertexPosition;",
		"attribute vec2 aTextureCoord;",
		"varying highp vec2 vTextureCoord;",
		"void main(void) {",
		" gl_Position = aVertexPosition;",
		" vTextureCoord = aTextureCoord;",
		"}"
	].join("\n");
	var vertexShader = gl.createShader(gl.VERTEX_SHADER);
	gl.shaderSource(vertexShader, vertexShaderSource);
	gl.compileShader(vertexShader);
	var fragmentShaderSource = [
		"precision highp float;",
		"varying lowp vec2 vTextureCoord;",
		"uniform sampler2D YTexture;",
		"uniform sampler2D UTexture;",
		"uniform sampler2D VTexture;",
		"const mat4 YUV2RGB = mat4",
		"(",
		" 1.1643828125, 0, 1.59602734375, -.87078515625,",
		" 1.1643828125, -.39176171875, -.81296875, .52959375,",
		" 1.1643828125, 2.017234375, 0, -1.081390625,",
		" 0, 0, 0, 1",
		");",
		"void main(void) {",
		" gl_FragColor = vec4( texture2D(YTexture, vTextureCoord).x, texture2D(UTexture, vTextureCoord).x, texture2D(VTexture, vTextureCoord).x, 1) * YUV2RGB;",
		"}"
	].join("\n");

	var fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
	gl.shaderSource(fragmentShader, fragmentShaderSource);
	gl.compileShader(fragmentShader);
	gl.attachShader(program, vertexShader);
	gl.attachShader(program, fragmentShader);
	gl.linkProgram(program);
	gl.useProgram(program);
	if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
		console.log("[ER] Shader link failed.");
	}
	var vertexPositionAttribute = gl.getAttribLocation(program, "aVertexPosition");
	gl.enableVertexAttribArray(vertexPositionAttribute);
	var textureCoordAttribute = gl.getAttribLocation(program, "aTextureCoord");
	gl.enableVertexAttribArray(textureCoordAttribute);

	var verticesBuffer = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, verticesBuffer);
	gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 1.0, -1.0, 0.0, -1.0, -1.0, 0.0]), gl.STATIC_DRAW);
	gl.vertexAttribPointer(vertexPositionAttribute, 3, gl.FLOAT, false, 0, 0);
	var texCoordBuffer = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, texCoordBuffer);
	gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0]), gl.STATIC_DRAW);
	gl.vertexAttribPointer(textureCoordAttribute, 2, gl.FLOAT, false, 0, 0);

	gl.y = new Texture(gl);
	gl.u = new Texture(gl);
	gl.v = new Texture(gl);
	gl.y.bind(0, program, "YTexture");
	gl.u.bind(1, program, "UTexture");
	gl.v.bind(2, program, "VTexture");
}

WebGLPlayer.prototype.initVideoParam = function (width, height, pixFmt) {
	this.width = width;
	this.height = height;
	this.pixFmt = pixFmt;
	this.uOffset = this.width * this.height;
	this.vOffset = (this.width / 2) * (this.height / 2);
}

WebGLPlayer.prototype.renderFrame = function (videoFrame) {
	if (!this.gl) {
		console.log("[ER] Render frame failed due to WebGL not supported.");
		return;
	}

	var gl = this.gl;
	// console.log(`w*h: ${gl.canvas.width} ${gl.canvas.height}`);

	gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);
	gl.clearColor(0.0, 0.0, 0.0, 0.0);
	gl.clear(gl.COLOR_BUFFER_BIT);

	gl.y.fill(this.width, this.height, videoFrame.subarray(0, this.uOffset));
	gl.u.fill(this.width >> 1, this.height >> 1, videoFrame.subarray(this.uOffset, this.uOffset + this.vOffset));
	gl.v.fill(this.width >> 1, this.height >> 1, videoFrame.subarray(this.uOffset + this.vOffset, videoFrame.length));

	gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
};

WebGLPlayer.prototype.fullscreen = function () {
	var canvas = this.canvas;
	if (canvas.RequestFullScreen) {
		canvas.RequestFullScreen();
	} else if (canvas.webkitRequestFullScreen) {
		canvas.webkitRequestFullScreen();
	} else if (canvas.mozRequestFullScreen) {
		canvas.mozRequestFullScreen();
	} else if (canvas.msRequestFullscreen) {
		canvas.msRequestFullscreen();
	} else {
		alert("This browser doesn't supporter fullscreen");
	}
};

WebGLPlayer.prototype.exitfullscreen = function () {
	if (document.exitFullscreen) {
		document.exitFullscreen();
	} else if (document.webkitExitFullscreen) {
		document.webkitExitFullscreen();
	} else if (document.mozCancelFullScreen) {
		document.mozCancelFullScreen();
	} else if (document.msExitFullscreen) {
		document.msExitFullscreen();
	} else {
		alert("Exit fullscreen doesn't work");
	}
}
WebGLPlayer.prototype.clear = function() {
    if (!this.gl) return;

    // 1. 清除画布内容
    const gl = this.gl;
    gl.clearColor(0.0, 0.0, 0.0, 1.0); // 黑色背景
    gl.clear(gl.COLOR_BUFFER_BIT);

    // 2. 可选：释放纹理资源（按需添加）
    if (gl.y && gl.y.texture) gl.deleteTexture(gl.y.texture);
    if (gl.u && gl.u.texture) gl.deleteTexture(gl.u.texture);
    if (gl.v && gl.v.texture) gl.deleteTexture(gl.v.texture);

    // 3. 重置尺寸参数（防止残留状态影响下次播放）
    this.width = 0;
    this.height = 0;
    this.uOffset = 0;
    this.vOffset = 0;
};


WebGLPlayer.prototype.destroy = function() {
    if (!this.gl) return;

    const gl = this.gl;

    // 1. 清除画布
    this.clear();

    // 2. 释放所有WebGL资源
    if (gl.y) gl.y.texture && gl.deleteTexture(gl.y.texture);
    if (gl.u) gl.u.texture && gl.deleteTexture(gl.u.texture);
    if (gl.v) gl.v.texture && gl.deleteTexture(gl.v.texture);

    // 3. 删除缓冲区和着色器程序（需在initGL中保存引用）
    if (this.verticesBuffer) gl.deleteBuffer(this.verticesBuffer);
    if (this.texCoordBuffer) gl.deleteBuffer(this.texCoordBuffer);
    if (this.program) {
        gl.deleteProgram(this.program);
        this.program = null;
    }

    // 4. 清除引用
    this.gl = null;
    this.canvas = null;
};
