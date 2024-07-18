#!/bin/bash

export BMEM_CACHEABLE=true
SCRIPT_DIR="$( cd "$( dirname "$(readlink -f "${BASH_SOURCE[0]}")" )" && pwd )"
MODELS_DIR=$SCRIPT_DIR/../
TOP_DIR=$MODELS_DIR/../
export LD_LIBRARY_PATH=$TOP_DIR/hbdk3/x5:$LD_LIBRARY_PATH
HBM_FILE=$MODELS_DIR/HBDK3_MODEL_P0000/x5/gen_P0000_MobileNetV1_1x224x224x3.hbm

OUTPUT_0_0=/userdata/model-p0000_00/
OUTPUT_0_1=/userdata/model-p0000_01/
OUTPUT_1_0=/userdata/model-p0000_10/
OUTPUT_1_1=/userdata/model-p0000_11/
SRC_FILE=$MODELS_DIR/HBDK3_MODEL_P0000/input_0_feature_1x224x224x3_ddr_native.bin
MODEL_NAME=P0000_MobileNetV1_1x224x224x3

function check_golden_result()
{
	count=0
	for output_files in $OUTPUT_0_0*.txt
	do
		ref_files=`echo $output_files | sed "s:$OUTPUT_0_0:./output_ref/:g"`
		diff $output_files $ref_files 
		
		# failure
		if [ $? -ne 0 ]; then
			echo "golden result ERROR!!"		
			return 1
		fi
	done

	# succeed
	echo "golden result PASS!!"
	return 0
}

if [ ! -d $OUTPUT_0_0 ];then
	mkdir $OUTPUT_0_0
else
	rm $OUTPUT_0_0 -r
	mkdir $OUTPUT_0_0
fi
if [ ! -d $OUTPUT_0_1 ];then
	mkdir $OUTPUT_0_1
else
	rm $OUTPUT_0_1 -r
	mkdir $OUTPUT_0_1
fi
if [ ! -d $OUTPUT_1_0 ];then
	mkdir $OUTPUT_1_0
else
	rm $OUTPUT_1_0 -r
	mkdir $OUTPUT_1_0
fi
if [ ! -d $OUTPUT_1_1 ];then
	mkdir $OUTPUT_1_1
else
	rm $OUTPUT_1_1 -r
	mkdir $OUTPUT_1_1
fi

if [ $# -eq 0 ]; then
	$TOP_DIR/tc_hbdk3 -f $HBM_FILE -i $SRC_FILE -n $MODEL_NAME -o $OUTPUT_0_0,$OUTPUT_0_1,$OUTPUT_1_0,$OUTPUT_1_1 -g 1 -c 2 -b 0

	check_golden_result
else
	$TOP_DIR/tc_hbdk3 -t $1 -b $2 -f $HBM_FILE -i $SRC_FILE -n $MODEL_NAME -o $OUTPUT_0_0,$OUTPUT_0_1,$OUTPUT_1_0,$OUTPUT_1_1 -g 0 -c 0
fi
