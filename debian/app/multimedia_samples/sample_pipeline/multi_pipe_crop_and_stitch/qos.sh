#!/bin/bash

ops_module=("20510500.sif0_qos" "7" "7"
	"20510580.sif1_qos" "7" "7"
	"20510600.sif2_qos" "7" "7"
	"20510680.sif3_qos" "7" "7"
	"20510280.isp_axi5_hdr_qos" "4" "4"
	"20510300.isp_axi4_mcm_qos" "4" "4"
	"20510380.isp_axi3_sp2_qos" "4" "4"
	"20510480.isp_axi1_mp_qos" "4" "4"
	"20510100.dw230_gdc_qos" "4" "4"
	"20510180.dw230_scalar2_qos" "4" "4"
	"20510200.dw230_scalar3_qos" "4" "4"
	"20520000.bpu_qos" "0" "0"
	"20510000.bt1120_qos" "7" "7"
	"20510080.dc8000_qos" "7" "7"
	"20530000.video_qos" "0" "0"
	"20530080.jpeg_qos" "0" "0")

function write_read_check_list() {
	local _i=0
	local _read_v=
	local _base_path="/sys/bus/platform/drivers/noc_qos"
	local _readp_path="read_priority_qos_ctrl/priority"
	local _writep_path="write_priority_qos_ctrl/priority"

	for (( _i=0;_i<${#ops_module[@]};_i+=3 )); do
		local _module=${ops_module[$_i]}
		local _read_qos=${ops_module[$_i+1]}
		local _write_qos=${ops_module[$_i+2]}

		if [ ! -f ${_base_path}/${_module}/${_readp_path} ] ||
		   [ ! -f ${_base_path}/${_module}/${_writep_path} ];then
			echo "${_module} QoS not initialized! Skipping!"
			continue
		fi

		echo ${_read_qos} > ${_base_path}/${_module}/${_readp_path}
		_read_v=$(cat ${_base_path}/${_module}/${_readp_path})
		if [[ "${_read_qos}" != "${_read_v: -1}" ]]; then
			echo "Config ${_module} read qos fail(${_read_qos} != ${_read_v: -1}), please check it"
		fi

		echo ${_write_qos} > ${_base_path}/${_module}/${_writep_path}
		_read_v=$(cat ${_base_path}/${_module}/${_writep_path})
		if [[ "${_write_qos}" != "${_read_v: -1}" ]]; then
			echo "Config ${_module} write qos fail(${_write_qos} != ${_read_v: -1}), please check it"
		fi

		echo "Config ${_module} qos read: ${_read_qos} write: ${_write_qos} done"
	done
}

write_read_check_list

echo ""
echo ""
echo ""

# 配置cpu bpu ddr 降频的结温温度
echo 105000 > /sys/class/thermal/thermal_zone0/trip_point_0_temp
echo 105000 > /sys/class/thermal/thermal_zone1/trip_point_1_temp

# 设置cpu运行在高性能模式
echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor


modprobe panel-jc-050hd134
modprobe galcore
modprobe vio_n2d
modprobe lontium_lt8618
modprobe vs-x5-syscon-bridge
modprobe vs_drm

