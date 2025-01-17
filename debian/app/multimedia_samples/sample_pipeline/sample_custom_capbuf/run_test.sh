#!/bin/bash

#set -x

export CAP_BUF_FLAG=0
export CAP_LOOP_CNT=100
echo "Using external capture buf loop 100 times"
# dump /userdata/vse
./sample_custom_capbuf -e vse -i ./test_res/test_image_1920x1080.yuv -w 1920 -h 1080
# dump /userdata/gdc
./sample_custom_capbuf -e gdc -c ./test_res/gdc.bin -i ./test_res/test_image_1920x1080.yuv -w 1920 -h 1080
# vse channel 0 bind gdc
./sample_custom_capbuf -e vse-0-gdc -i ./test_res/test_image_1920x1080.yuv -w 1920 -h1080 -c ./test_res/gdc.bin
# gdc channel 0 bind vse
./sample_custom_capbuf -e gdc-0-vse -i ./test_res/test_image_1920x1080.yuv -w 1920 -h1080 -c ./test_res/gdc.bin

export CAP_BUF_FLAG=2
export CAP_LOOP_CNT=100
# dump /userdata/vse
echo "Using internal capture buf loop 100 times"
./sample_custom_capbuf -e vse -i ./test_res/test_image_1920x1080.yuv -w 1920 -h 1080
# dump /userdata/gdc
./sample_custom_capbuf -e gdc -c ./test_res/gdc.bin -i ./test_res/test_image_1920x1080.yuv -w 1920 -h 1080
# vse channel 0 bind gdc
./sample_custom_capbuf -e vse-0-gdc -i ./test_res/test_image_1920x1080.yuv -w 1920 -h1080 -c ./test_res/gdc.bin
# gdc channel 0 bind vse
./sample_custom_capbuf -e gdc-0-vse -i ./test_res/test_image_1920x1080.yuv -w 1920 -h1080 -c ./test_res/gdc.bin

export CAP_BUF_FLAG=0
export CAP_LOOP_CNT=20
export CAP_LOOP_FLAG=1
export CAP_THREAD_NUM=6
./sample_custom_capbuf -e vse -i ./test_res/test_image_1920x1080.yuv -w 1920 -h 1080
# dump /userdata/gdc
./sample_custom_capbuf -e gdc -c ./test_res/gdc.bin -i ./test_res/test_image_1920x1080.yuv -w 1920 -h 1080

export CAP_BUF_FLAG=0
export CAP_LOOP_CNT=20
export CAP_LOOP_FLAG=0
export CAP_THREAD_NUM=6
./sample_custom_capbuf -e vse -i ./test_res/test_image_1920x1080.yuv -w 1920 -h 1080
# dump /userdata/gdc
./sample_custom_capbuf -e gdc -c ./test_res/gdc.bin -i ./test_res/test_image_1920x1080.yuv -w 1920 -h 1080
