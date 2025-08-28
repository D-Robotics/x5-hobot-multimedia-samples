

const configFieldInfoTable = {
	"pipeline_count": {
		chinese_name: "视频通路数",
		type: "maxlimit",
		options: "max_pipeline_count"
	},
	"sensor": {
		chinese_name: "Sensor型号",
		type: "stringlist",
		options: "sensor_list", // Placeholder for sensor options
		value_is_index: false
	},
	"encode_type": {
		chinese_name: "编码类型",
		type: "stringlist",
		options: "codec_type_list",
		value_is_index: true
	},
	"encode_bitrate": {
		chinese_name: "编码码率",
		type: "intarray",
		options: "encode_bit_rate_list"
	},
	"model": {
		chinese_name: "算法模型",
		type: "stringlist",
		options: "model_list", // Placeholder for model options
		value_is_index: false
	},
	"gdc_status": {
		chinese_name: "使能GDC",
		type: "stringlist",
		options: "gdc_status_list",
		value_is_index: true
	},
	"stream": {
		chinese_name: "视频数据流",
		type: "text",
		value: "../test_data/1080P_test.h264" // Placeholder for stream value
	},
	"decode_type": {
		chinese_name: "解码类型",
		type: "stringlist",
		options: "codec_type_list",
		value_is_index: true
	},
	"decode_width": {
		chinese_name: "解码宽度",
		type: "int",
		value: 1920 // Placeholder for decode_width value
	},
	"decode_height": {
		chinese_name: "解码高度",
		type: "int",
		value: 1080 // Placeholder for decode_height value
	},
	"decode_frame_rate": {
		chinese_name: "解码帧率",
		type: "int",
		value: 30 // Placeholder for decode_frame_rate value
	},
	"encode_width": {
		chinese_name: "编码宽度",
		type: "int",
		value: 1920 // Placeholder for encode_width value
	},
	"encode_height": {
		chinese_name: "编码高度",
		type: "int",
		value: 1080 // Placeholder for encode_height value
	},
	"encode_frame_rate": {
		chinese_name: "编码帧率",
		type: "int",
		value: 30 // Placeholder for encode_frame_rate value
	}
};
class ConfigManager {
    constructor() {
        this.serverConfig = {};
		this.userCallbacks = {}; // 存储用户定义的回调函数
		this.configFieldInfoTable = configFieldInfoTable;
		this.isConfigVisibility= 0;
    }

	init(callbacks = {}){
		this.userCallbacks = callbacks;
	}

	updateConfig(config){
		this.serverConfig = config;
	}
	getConfigWithJson(){
		return JSON.stringify(this.serverConfig);
	}
	getStreamCodecType(){
		let codec_types = [];
		const { solution_name, cam_solution, box_solution} = this.serverConfig;
		if (solution_name === 'cam_solution') {
			for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
				const cam_vpp = cam_solution.cam_vpp[i];
				if (cam_vpp.is_enable === 0) {
					continue;
				}
				const codec_type_string = this._getStringDecodeType(cam_vpp["encode_type"]);
				if (codec_type_string === "unsupport") {
					continue;
				}
				codec_types.push(codec_type_string);
			}
		} else if (solution_name === 'box_solution') {
			const pipeline_count = box_solution["pipeline_count"];
			const box_vpp_list = box_solution["box_vpp"];
			for (let i = 0; i < pipeline_count; i++) {
				const codec_type_string = this._getStringDecodeType(box_vpp_list[i]["encode_type"]);
				if (codec_type_string === "unsupport") {
					continue;
				}
				codec_types.push(codec_type_string);
			}
		}else{
			console.warn(`not support soulution name [${solution_name}]`);
			return [];
		}
		return codec_types;
	}

	/*										API接口							 */
    /**
     * 动态构造 HTML
     * @param {Object} config 服务器返回的配置
     */
    buildHTMLFromConfig(isFourceUpdateWindow) {
		const { solution_name } = this.serverConfig;

		const container = document.getElementById('solutionConfig');
		let html = '';
		html += this.generateDeviceInfoHTML(this.serverConfig);

		html += this.generateSelectAppSolutionHTML(this.serverConfig);
		if (solution_name === 'cam_solution') {
			let solution = this.serverConfig["cam_solution"]
			let hardware_capability = this.serverConfig["hardware_capability"];
			html += this.generateCamSolutionHtml(solution, hardware_capability);
			container.innerHTML = html;
			this.bindCamSolutionEvents(solution);
		} else if (solution_name === 'box_solution') {
			let solution = this.serverConfig["box_solution"]
			html += this.generateBoxSolutionHtml(solution);
			container.innerHTML = html;
			this.bindBoxSolutionEvents(solution);
		} else {
			console.error("不支持的解决方案类型: " + solution_name);
		}
		this.buildSelectAppSolutionEvents();// 必须在 container.innerHTML的后面

		this.generateConfigureDescribeHTML(this.serverConfig);
		this.bindConfigureDescribeEvent()


		if(this.userCallbacks.onChange){
			let stream_count = this._streamCount();
			this.userCallbacks.onChange(stream_count, isFourceUpdateWindow); //更新视频区域的布局
		}else{
			console.log("not listen html change event.");
		}
		console.log("html updated.");
	}

	// ==================== [1. 设备信息] ====================
	generateDeviceInfoHTML(servier_config) {
		const { solution_name, hardware_capability, version } = servier_config;
		let html = `<h2>设备信息</h2>`;
		html += `<span style="white-space: pre-wrap;"><strong>芯片类型 : </strong>${hardware_capability["chip_type"]}  </span>`;
		html += `<span style="white-space: pre-wrap;"><strong>软件版本 : </strong>${version}  </span>`;

		if (solution_name === 'cam_solution') {
			html += this.generateCamSolutionDeviceInfoHTML(servier_config["cam_solution"]);
		} else if (solution_name === 'box_solution') {
			html += this.generateBoxSolutionDeviceInfoHTML(servier_config["box_solution"]);
		} else {
			console.error("不支持的解决方案类型: " + solution_name);
		}

		return html;
	}

	generateCamSolutionDeviceInfoHTML(cam_solution) {
		let html = '';
		let valid_index = 0;

		for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
			const cam_vpp = cam_solution.cam_vpp[i];
			if (cam_vpp.is_enable === 0) {
				continue;
			}
			const cameraChannelName = 'CSI_' + cam_vpp.csi_index;

			// 通道标题（加粗）
			html += `<div class="channel-header"><strong>${cameraChannelName}的码流链接 : </strong></div>`;

			// 主码流（缩进）
			html += `<div class="stream-url">`;
			html += `<span class="stream-type">主码流：</span>`;
			html += `<span class="stream-uri">rtsp://${window.location.host}/ch${valid_index}/main</span>`;
			html += `</div>`;

			// 子码流（缩进）
			html += `<div class="stream-url">`;
			html += `<span class="stream-type">子码流：</span>`;
			html += `<span class="stream-uri">rtsp://${window.location.host}/ch${valid_index}/sub1</span>`;
			html += `</div>`;

			// 通道间间隔
			html += `<div style="height: 8px;"></div>`;

			valid_index++;
		}

		return html;
	}
	generateBoxSolutionDeviceInfoHTML(box_solution) {
		const pipeline_count = box_solution["pipeline_count"];
		const box_vpp_list = box_solution["box_vpp"];
		let html = '';

		for (let i = 0; i < pipeline_count; i++) {
			html += `<div class="channel-header"><strong> 第${i}路的码流链接 : </strong></div>`;
			
			// 主码流（缩进）
			html += `<div class="stream-url">`;
			html += `<span class="stream-type">主码流：</span>`;
			html += `<span class="stream-uri">rtsp://${window.location.host}/ch${i}/main</span>`;
			html += `</div>`;

			// 子码流（缩进）
			html += `<div class="stream-url">`;
			html += `<span class="stream-type">子码流：</span>`;
			html += `<span class="stream-uri">rtsp://${window.location.host}/ch${i}/sub1</span>`;
			html += `</div>`;

			// 通道间间隔
			html += `<div style="height: 8px;"></div>`;
		}

		return html;
	}

	// ==================== [2. 选择应用方案] ====================
	generateSelectAppSolutionHTML(solutions_config){
		const { solution_name } = solutions_config;

		let html = `<h2>选择应用方案</h2>
		<form id="solutionForm">
			<label for="cam_solution">
				<input type="radio" id="cam_solution" name="solution" value="cam_solution"
					${solution_name === "cam_solution" ? 'checked' : ''}> 智能摄像机
				<a href="#" class="info-icon" id="info-cam" data-solution="cam_solution">?</a>
			</label>
			<label for="box_solution">
				<input type="radio" id="box_solution" name="solution" value="box_solution"
					${solution_name === "box_solution" ? 'checked' : ''}> 智能分析盒
				<a href="#" class="info-icon" id="info-box" data-solution="box_solution">?</a>
			</label>
		</form>`;
		return html;
	}
	buildSelectAppSolutionEvents(){
		const camSolutionInfo = document.getElementById('info-cam');
        if (camSolutionInfo) {
            camSolutionInfo.addEventListener('click', (e) => {
                window.open("cam_solution_info.html", "_blank");
            });
        }
		const boxSolutionInfo = document.getElementById('info-box');
        if (boxSolutionInfo) {
            boxSolutionInfo.addEventListener('click', (e) => {
                window.open("box_solution_info.html", "_blank");
            });
        }

		const camSolutionElement = document.getElementById('cam_solution');
        if (camSolutionElement) {
            camSolutionElement.addEventListener('click', () => this._handleSolutionChange('cam_solution', 'image/camera-slt.jpg'));
        }

		const boxSolutionElement = document.getElementById('box_solution');
        if (boxSolutionElement) {
            boxSolutionElement.addEventListener('click', () => this._handleSolutionChange('box_solution', 'image/box-slt.jpg'));
        }
	}
	// ==================== [3. 智能摄像机]               ====================
	generateCamSolutionHtml(cam_solution, hardware_capability) {
		let html = `<div><strong>智能摄像机</strong><ul>`;

		// 3.1 使能Camera接口: 遍历所有的配置，添加 有效的CSI_X 对应的html
		if (cam_solution.pipeline_count > 0) {
			html += '<div><strong>使能 Camera 接口:</strong><div class="camera_channel_control">';
		}
		const cam_vpp_list = cam_solution.cam_vpp;

		for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
			const cam_vpp = cam_vpp_list[i];
			if (cam_vpp.is_valid === 0) continue;

			const checkboxName = 'CSI_' + cam_vpp.csi_index;
			html += `<div class="checkbox-item">
						<input type="checkbox" id="checkbox${i}" name="${checkboxName}" ${cam_vpp.is_enable ? 'checked' : ''}>
						<label for="checkbox${i}">${checkboxName}</label>
					</div>`;
		}
		html += '</div></div><br>';

		// 3.2 Camera接口(CSIx)：遍历所有的配置，为步骤2.2中选择的通道添加详细配置
		for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
			const cam_vpp = cam_vpp_list[i];
			if (cam_vpp.is_enable === 0 || cam_vpp.is_valid === 0) continue;

			html += `<li style="display: inline-block;"><strong>Camera 接口(CSI${cam_vpp.csi_index}):</strong><ul>`;
			for (const itemKey in cam_vpp) {
				const uniqueId = `item_${i}_${itemKey}`;
				if (itemKey === "sensor") {
					/**
					 * sensor 的配置在 hardware_capability.csi_list_info[i].sensor_config_list
					 * 所以与函数 render_label_name 处理过程不兼容,所以此处复制 一份处理
					 */
					const field = this.configFieldInfoTable[itemKey];
					const label = field ? `${field.chinese_name}（${itemKey}）` : itemKey;
					html += `<li><span>${label}</span>：`;

					html += `<select id="${uniqueId}" class="form-control-sm">`;
					const csi_list_info = hardware_capability.csi_list_info;
					const csi_info = csi_list_info.csi_info[i];
					const options = csi_info.sensor_config_list.split('/');
					options.forEach(option => {
						html += `<option value="${option}" ${cam_vpp["sensor"] === option ? 'selected' : ''}>${option}</option>`;
					});
					html += `</select>`;
					html += `</li>`;
				}else if((itemKey === "csi_index") || (itemKey === "is_enable") || (itemKey === "is_valid") ||  (itemKey === "mclk_is_not_configed") ){
					continue;
				}else if(itemKey === "gdc_status"){
					if(cam_vpp.gdc_status === -1){
						const textValue = "invalid";
						const field = this.configFieldInfoTable[itemKey];
						const label = ` ${field.chinese_name}（${itemKey}）`;
						html += `<li><span>${label}</span>：`;
						html += `<input type="text" id="${uniqueId}" value=" ${textValue}">`;
						html += `</li>`;
					}else{
						html += this._renderLabelName(this.serverConfig, itemKey, uniqueId, cam_vpp);
					}
				}else{
					html += this._renderLabelName(this.serverConfig, itemKey, uniqueId, cam_vpp);
				}
			}
			html += `</ul></li>`;
		}
		html += `</ul></div>`;

		return html;
	}
	bindCamSolutionEvents(cam_solution) {
        const cam_vpp_list = cam_solution.cam_vpp;

        for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
            const cam_vpp = cam_vpp_list[i];
            if (cam_vpp.is_valid === 0) continue;

            document.getElementById(`checkbox${i}`).addEventListener("change", this._createCheckboxChangeHandler(i));
        }

        // for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
        //     const cam_vpp = cam_vpp_list[i];
        //     if (cam_vpp.is_enable === 0) continue;

        //     const uniqueId = `item_${i}_encode_type`;
        //     document.getElementById(uniqueId).addEventListener("change", this._encodeTypeCamSolutionChangeHandler(i));
        // }
    }
	// ==================== [4. 智能分析盒]               ====================
	generateBoxSolutionHtml(box_solution) {
		//4.1 智能分析盒：视频通道路数
        let html = `<div><strong>智能分析盒</strong><ul>`;
        const field = this.configFieldInfoTable["pipeline_count"];
        const label = field ? `${field.chinese_name}（pipeline_count）` : "pipeline_count";
        html += `<div"><span>${label}</span>：<select id="item_box_pipeline_count" class="form-control-sm">`;
        for (let option = 1; option <= box_solution[field.options]; option++) {
            html += `<option value="${option}" ${box_solution["pipeline_count"] === option ? 'selected' : ''}>${option}</option>`;
        }
        html += `</select></div>`;

		// 4.2 第 x 路配置
        for (let i = 0; i < box_solution["pipeline_count"]; i++) {
            html += `<li style="display: inline-block;"><strong>第 ${i + 1} 路配置：</strong><ul>`;
            for (const itemKey in box_solution["box_vpp"][i]) {
                const uniqueId = `item_${i}_${itemKey}`;
                html += this._renderLabelName(this.serverConfig, itemKey, uniqueId, box_solution["box_vpp"][i]);
            }
            html += `</ul></li>`;
        }
        html += `</ul></div>`;

        return html;
    }

	bindBoxSolutionEvents(box_solution) {
        document.getElementById("item_box_pipeline_count").addEventListener("change", () => {
            const selectedPipelineCount = parseInt(document.getElementById("item_box_pipeline_count").value);
            this.serverConfig["box_solution"]["pipeline_count"] = selectedPipelineCount;
            this.buildHTMLFromConfig(false);
        });
		//意义：点击选项后，最左侧的当前方案配置栏 会在提交前更新
        // for (let i = 0; i < box_solution.pipeline_count; i++) {
        //     const uniqueId = `item_${i}_encode_type`;
        //     document.getElementById(uniqueId).addEventListener("change", this._encodeTypeBoxSolutionChangeHandler(i));
        // }
    }
	// ==================== [5. 当前方案配置]               ====================
	generateConfigureDescribeHTML(solution){
		const solution_status = document.getElementById("solution_status");
        if (!solution_status) {
            console.error("状态显示元素未找到！");
            return;
        }

        const solution_name = solution["solution_name"];
        let html = "<h4>当前方案配置：</h4>";

        if (solution_name === 'cam_solution') {
            html += this.generateCamSolutionConfigureDescribeHTML(solution["cam_solution"]);
        } else if (solution_name === 'box_solution') {
            html += this.generateBoxSolutionConfigureDescribeHTML(solution["box_solution"]);
        }
        solution_status.innerHTML = html;
        solution_status.classList.add('left-align');
	}
	bindConfigureDescribeEvent() {
        const imageElement = document.getElementById("solution_image");
        if (!imageElement) return;

        imageElement.addEventListener("click", () => this._showImageModal(imageElement.src));
    }

	generateCamSolutionConfigureDescribeHTML(cam_solution) {
        const pipeline_count = cam_solution["pipeline_count"];
        const max_pipeline_count = cam_solution["max_pipeline_count"];

        let enable_count = 0;
        for (let i = 0; i < max_pipeline_count; i++) {
            if (cam_solution["cam_vpp"][i]["is_enable"] !== 0) {
                enable_count++;
            }
        }

        let html = "<strong>智能摄像机：</strong></br>";
        html += `- 接入 ${pipeline_count} 路Sensor</br>`;
        html += `- 启用 ${enable_count} 路Sensor</br>`;
        html += "<strong>启用的Sensor型号和算法模型：</strong></br>";

        let valid_index = 0;
        for (let i = 0; i < max_pipeline_count; i++) {
            const cam_vpp = cam_solution["cam_vpp"][i];
            if (cam_vpp["is_enable"] === 0) continue;

            const sensor_model = cam_vpp["sensor"];
            const algorithm_model = cam_vpp["model"];
			const encode_type = this._getStringDecodeType(cam_vpp["encode_type"]);
            html += `- 第 ${valid_index + 1} 路:`;
            html += `<ul>`;
            html += `<li>Sensor型号：${sensor_model}</li>`;
			html += `<li>编码类型：${encode_type.toUpperCase()}</li>`;
            html += `<li>算法：${algorithm_model}</li>`;
            html += `</ul>`;
            valid_index++;
        }

        html += "<strong>方案框图（点击放大）</strong></br>";
        html += `<img id="solution_image" src="image/camera-slt.jpg" style="display: block;max-width:100%; max-height:100%" />`;

        return html;
    }

    /**
     * 生成智能分析盒方案的状态文本
     * @returns {string} - 状态文本
     */
    generateBoxSolutionConfigureDescribeHTML(box_solution) {
        const pipeline_count = box_solution["pipeline_count"];

        let status_txt = "<strong>智能分析盒：</strong></br>";
        status_txt += `- 启用 ${pipeline_count} 路视频</br>`;
        status_txt += "<strong>编解码和算法模型：</strong></br>";

        for (let i = 0; i < pipeline_count; i++) {
            const box_vpp = box_solution["box_vpp"][i];
            const decode_resolution = this._formatResolution(box_vpp, "decode");
            const encode_resolution = this._formatResolution(box_vpp, "encode");
            const algorithm_model = box_vpp["model"];

            status_txt += `- 第 ${i + 1} 路:`;
            status_txt += `<ul>`;
            status_txt += `<li>解码：${decode_resolution}</li>`;
            status_txt += `<li>编码：${encode_resolution}</li>`;
            status_txt += `<li>算法：${algorithm_model}</li>`;
            status_txt += `</ul>`;

        }

        status_txt += "<strong>方案框图（点击放大）</strong></br>";
        status_txt += `<img id="solution_image" src="image/box-slt.jpg" style="display: block;max-width:100%; max-height:100%" />`;

        return status_txt;
    }

	/*										API接口							 */
    /**
     * 更新服务器配置，基于 HTML 控件的值
     * @returns {Object} 更新后的配置
     */
    updateConfigFromHTML() {
		const solutionName = document.querySelector('input[name="solution"]:checked').value;
		this.serverConfig["solution_name"] = solutionName;
		if (solutionName === 'cam_solution') {
			this.updateCamSolution(this.serverConfig["cam_solution"]);
		} else if (solutionName === 'box_solution') {
			this.updateBoxSolution(this.serverConfig["box_solution"]);
		}
	}

	updateCamSolution(cam_solution){
		// 更新 复选框导致的更新
		// 忽略,在复选框的事件处理函数中已经更新
		// 更新 cam_vpp
		const cam_vpp_list = cam_solution.cam_vpp
		for (let i = 0; i < cam_solution.max_pipeline_count; i++) {
			const cam_vpp = cam_vpp_list[i];
			if(cam_vpp.is_valid === 0){
				continue;
			}
			if (cam_vpp.is_enable === 0) {
				console.log("ignore ->index:" + i);
				continue;
			}
			// console.log("index:" + i);

			for (const itemKey in cam_solution["cam_vpp"][i]) {
				if(itemKey === "csi_index")
					continue;
				if(itemKey === "is_enable")
					continue;
				if(itemKey === "is_valid")
					continue;
				if(itemKey === "mclk_is_not_configed")
					continue;

				if((itemKey === 'gdc_status') && (cam_solution["cam_vpp"][i][itemKey] === -1)){
					continue;
				}

				const uniqueId = `item_${i}_${itemKey}`;
				const element = document.getElementById(uniqueId);
				if (element.tagName === "INPUT") {
					// 根据输入元素的类型更新字段值
					if (element.type === "number") {
						cam_solution["cam_vpp"][i][itemKey] = parseInt(element.value);
					} else {
						cam_solution["cam_vpp"][i][itemKey] = element.value;
					}
				} else if (element.tagName === "SELECT") {
					// 获取选中的选项索引
					const selectedIndex = element.selectedIndex;
					const selectedValue = element.value.trim();
					// 检查选择的值是否是一个有效的数字
					const numericValue = parseInt(selectedValue);
					if (!isNaN(numericValue)) {
						// 如果是数字，则按照数字处理
						// 更新 JSON 中对应字段的值
						cam_solution["cam_vpp"][i][itemKey] = numericValue;
					} else {
						// 如果不是数字，则按照字符串处理
						// 对于 encode_type 或 decode_type，读取下拉选择框的编号值
						if (itemKey === 'encode_type' || itemKey === 'decode_type') {
							cam_solution["cam_vpp"][i][itemKey] = parseInt(selectedIndex);
						}else if (itemKey === 'gdc_status'){
							if(cam_solution["cam_vpp"][i][itemKey] === -1){
							}else{
								cam_solution["cam_vpp"][i][itemKey] = parseInt(selectedIndex);
							}
						}else {
							// 对于其他字段，直接更新为选择的文本值
							cam_solution["cam_vpp"][i][itemKey] = selectedValue;
						}
					}
				}
			}
		}
	}

	updateBoxSolution(box_solution){
		// 更新 pipeline_count
		const pipelineCountSelect = document.getElementById("item_box_pipeline_count");
		box_solution["pipeline_count"] = parseInt(pipelineCountSelect.value);

		// 更新 box_vpp
		for (let i = 0; i < box_solution["pipeline_count"]; i++) {
			for (const itemKey in box_solution["box_vpp"][i]) {
				const uniqueId = `item_${i}_${itemKey}`;
				const element = document.getElementById(uniqueId);
				if (element.tagName === "INPUT") {
					// 根据输入元素的类型更新字段值
					if (element.type === "number") {
						box_solution["box_vpp"][i][itemKey] = parseInt(element.value);
					} else {
						box_solution["box_vpp"][i][itemKey] = element.value;
					}
				} else if (element.tagName === "SELECT") {
					// 获取选中的选项索引
					const selectedIndex = element.selectedIndex;
					const selectedValue = element.value.trim();
					// 检查选择的值是否是一个有效的数字
					const numericValue = parseInt(selectedValue);
					if (!isNaN(numericValue)) {
						// 如果是数字，则按照数字处理
						// 更新 JSON 中对应字段的值
						box_solution["box_vpp"][i][itemKey] = numericValue;
					} else {
						// 如果不是数字，则按照字符串处理
						// 对于 encode_type 或 decode_type，读取下拉选择框的编号值
						if (itemKey === 'encode_type' || itemKey === 'decode_type') {
							box_solution["box_vpp"][i][itemKey] = parseInt(selectedIndex);
						} else {
							// 对于其他字段，直接更新为选择的文本值
							box_solution["box_vpp"][i][itemKey] = selectedValue;
						}
					}
				}
			}
		}
	}

	/* ------------------------ 回调函数  ------------------------------ */
	onToggleVisibility(){
		var solution_configs = document.getElementById("solution_configs");
		if (this.isConfigVisibility == 0){
			solution_configs.style.display = "table";
		}else{
			solution_configs.style.display = "none";
		}
		this.isConfigVisibility = this.isConfigVisibility ? 0 : 1;
	}

	/* ------------------------ 工具函数  ------------------------------ */
	_createCheckboxChangeHandler(checkboxNumber) {
        return (event) => {
            const cam_solution = this.serverConfig["cam_solution"];
            const cam_vpp_list = cam_solution.cam_vpp;
            const cam_vpp = cam_vpp_list[checkboxNumber];

            if (cam_vpp.is_valid === 0) {
                console.error("CSI_" + cam_vpp.csi_index + " 无效，但尝试设置复选框。");
                return;
            }

            cam_vpp.is_enable = event.target.checked ? 1 : 0;
            this.buildHTMLFromConfig(false); // 重新渲染
        };
    }
	_encodeTypeCamSolutionChangeHandler(channel_number) {
        return (event) => {
            const cam_solution = this.serverConfig["cam_solution"];
            const cam_vpp = cam_solution.cam_vpp[channel_number];

            if (cam_vpp.is_enable === 0) {
                console.error("CSI_" + cam_vpp.csi_index + " 未启用，但尝试设置编码类型。");
                return;
            }

            const selectedEncodeType = event.target.options[event.target.selectedIndex].text;
            const encode_type_int = this._getIntDecodeType(selectedEncodeType);
            if (encode_type_int === -1) {
                console.error("CSI_" + cam_vpp.csi_index + " 收到不支持的编码类型: " + selectedEncodeType);
                return;
            }

            cam_vpp.encode_type = encode_type_int;
			this.buildHTMLFromConfig(false);
		};
	}

	_encodeTypeBoxSolutionChangeHandler(channel_number) {
        return (event) => {
            const box_solution = this.serverConfig["box_solution"];
            const box_vpp_list = box_solution.box_vpp;
            const box_vpp = box_vpp_list[channel_number];

            // 获取选中的编码类型
            const selectedEncodeType = event.target.options[event.target.selectedIndex].text;
            const encode_type_int = this._getIntDecodeType(selectedEncodeType);

            // 检查编码类型是否有效
            if (encode_type_int === -1) {
                console.error(`通道 ${channel_number} 收到不支持的编码类型: ${selectedEncodeType}`);
                return;
            }

            // 更新配置中的编码类型
            box_vpp["encode_type"] = encode_type_int;

            // 重新渲染页面
            this.buildHTMLFromConfig(false);
        };
    }

	/**
     * 渲染标签名称和对应的输入控件
     * @param {string} itemKey - 配置项键名
     * @param {string} uniqueId - 控件的唯一 ID
     * @param {object} vpp_config - 配置项的值
     * @returns {string} - 生成的 HTML
     */
    _renderLabelName(solutions_config, itemKey, uniqueId, vpp_config) {
        const field = this.configFieldInfoTable[itemKey];
        const label = field ? `${field.chinese_name}（${itemKey}）` : itemKey;
        let html = `<li><span>${label}</span>：`;

        if (field) {
            switch (field.type) {
                case 'stringlist':
                    html += this._generateSelectOptions(solutions_config, field, uniqueId, vpp_config[itemKey]);
                    break;
                case 'intarray':
                    html += this._generateBitrateOptions(solutions_config, field, uniqueId, vpp_config[itemKey]);
                    break;
                case 'text':
                    html += `<input type="text" id="${uniqueId}" value="${vpp_config[itemKey] || field.value}">`;
                    break;
                case 'int':
                    html += `<input type="number" id="${uniqueId}" value="${vpp_config[itemKey] || field.value}" step="1">`;
                    break;
                default:
                    html += `<input type="text" id="${uniqueId}" value="${vpp_config[itemKey]}">`;
            }
        } else {
			console.log("uniqueId:" + uniqueId + " " + "itemKey: " + itemKey );
            html += `<input type="text" id="${uniqueId}" value="${vpp_config[itemKey]}">`;
        }

        html += `</li>`;
        return html;
    }

    /**
     * 生成下拉列表选项
     * @param {object} field - 配置字段信息
     * @param {string} uniqueId - 控件的唯一 ID
     * @param {string|number} selectedValue - 当前选中的值
     * @returns {string} - 生成的 HTML
     */
    _generateSelectOptions(solutions_config, field, uniqueId, selectedValue) {
		const hardware_capability = solutions_config["hardware_capability"];
        const options = hardware_capability[field.options].split('/');
        let html = `<select id="${uniqueId}" class="form-control-sm">`;

        if (Array.isArray(options)) {
            if (field.value_is_index) {
                // 如果值是索引
                const index = Math.min(selectedValue, options.length - 1);
                if (selectedValue >= options.length) {
                    console.error(`itemKey: ${itemKey} 在 hardware_capability: ${field.options} 中的索引 ${selectedValue} 超出范围，最大为 ${options.length - 1}`);
                }
                options.forEach((option, i) => {
                    html += `<option value="${option}" ${i === index ? 'selected' : ''}>${option}</option>`;
                });
            } else {
                // 如果值是字符串
                options.forEach(option => {
                    html += `<option value="${option}" ${option === selectedValue ? 'selected' : ''}>${option}</option>`;
                });
            }
        } else {
            console.error(`itemKey: ${itemKey} 在 hardware_capability: ${field.options} 中的值不是数组`);
			const selected_value = hardware_capability[field.options]
			html += `<option value="${selected_value}" selected>${selected_value}</option>`;
        }

        html += `</select>`;
        return html;
    }

    /**
     * 生成码率下拉列表选项
     * @param {object} field - 配置字段信息
     * @param {string} uniqueId - 控件的唯一 ID
     * @param {number} selectedValue - 当前选中的值
     * @returns {string} - 生成的 HTML
     */
    _generateBitrateOptions(solutions_config, field, uniqueId, selectedValue) {
		const hardware_capability = solutions_config["hardware_capability"];
        const options = hardware_capability[field.options];
        let html = `<select id="${uniqueId}" class="form-control-sm">`;

        options.forEach(option => {
            if (option > 0) {
                html += `<option value="${option}" ${option === selectedValue ? 'selected' : ''}>${option}Kbps</option>`;
            }
        });

        html += `</select>`;
        return html;
    }

	_getStringDecodeType(encode_type) {
		const codecMap = {
			0: "h264",
			1: "h265",
			2: "mjpeg",
		};
		return codecMap[encode_type] || "unsupport";
	}

	_getIntDecodeType(decode_type_string) {
		const decodeTypeMap = new Map([
			["H264", 0],
			["H265", 1],
			["MJPEG", 2],
		]);
		const decodeType = decodeTypeMap.get(decode_type_string);
		if (decodeType === undefined) {
			console.error("收到不支持的编码类型: " + decode_type_string);
			return -1;
		}
		return decodeType;
	}
	_formatResolution(vpp, type) {
        const width = vpp[`${type}_width`];
        const height = vpp[`${type}_height`];
        const frame_rate = vpp[`${type}_frame_rate`];
        return `${width}x${height}@${frame_rate}fps`;
    }
    _showImageModal(imageSrc) {
        const modal = document.createElement("div");
        modal.style.position = "fixed";
        modal.style.top = "0";
        modal.style.left = "0";
        modal.style.width = "100%";
        modal.style.height = "100%";
        modal.style.backgroundColor = "rgba(0,0,0,0.7)";
        modal.style.zIndex = "1000";
        modal.style.display = "flex";
        modal.style.alignItems = "flex-start";
        modal.style.justifyContent = "center";

        const enlargedImage = document.createElement("img");
        enlargedImage.src = imageSrc;
        enlargedImage.style.maxWidth = "90%";
        enlargedImage.style.maxHeight = "90%";

        const closeButton = document.createElement("button");
        closeButton.textContent = "关闭";
        closeButton.style.marginLeft = "10px";
        closeButton.style.padding = "5px 10px";
        closeButton.style.border = "none";
        closeButton.style.backgroundColor = "#ffffff";
        closeButton.style.cursor = "pointer";
        closeButton.addEventListener("click", () => modal.remove());

        modal.appendChild(enlargedImage);
        modal.appendChild(closeButton);
        document.body.appendChild(modal);
    }

    /**
     * 处理解决方案切换
     * @param {string} selectedSolution - 选中的解决方案名称
     * @param {string} imageSrc - 方案对应的图片路径
     */
    _handleSolutionChange(selectedSolution, imageSrc) {
        // 更新选中的解决方案名称
        this.serverConfig["solution_name"] = selectedSolution;

        // 更新方案图片
        const solution_image = document.getElementById("solution_image");
        if (solution_image) {
            solution_image.style.display = "block";
            solution_image.setAttribute("src", imageSrc);
        }

        // 重新渲染页面
        this.buildHTMLFromConfig(false);
    };

	_streamCount() {
		let stream_count = 0;
		const { solution_name } = this.serverConfig;
		if (solution_name === 'cam_solution') {
			let solution = this.serverConfig["cam_solution"];
			const cam_vpp_list = solution.cam_vpp
			for (let i = 0; i < solution.max_pipeline_count; i++) {
				const cam_vpp = cam_vpp_list[i];
				if (cam_vpp.is_enable === 0) {
					continue;
				}
				if(cam_vpp.is_valid === 0){
					console.error('channel:' + i + "is not valid, but cam app is enable:" + cam_vpp.sensor);
					continue;
				}
				stream_count += 1;
			}
		} else if (solution_name === 'box_solution') {
			let solution = this.serverConfig["box_solution"]
			stream_count = solution["pipeline_count"];
		} else {
			console.error("不支持的解决方案类型: " + solution_name);
		}
		return stream_count;
	}
}

export default ConfigManager;
