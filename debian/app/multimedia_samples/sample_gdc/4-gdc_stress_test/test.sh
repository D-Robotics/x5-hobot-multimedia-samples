#!/bin/sh

./gdc_stress_test -c ./gdc_1920x1080.bin \
	-i ./test_res/test_image_1920x1080.yuv \
	-o ./gdc_output_1920x1080.yuv -w 1920 -h 1080 \
	-p process1 -C 10000 & \
./gdc_stress_test -c ./gdc_1920x1080.bin \
	-i ./test_res/test_image_1920x1080.yuv \
	-o ./gdc_output_1920x1080.yuv -w 1920 -h 1080 \
	-p process2 -C 10000 & \
./gdc_stress_test -c ./gdc_1920x1080.bin \
	-i ./test_res/test_image_1920x1080.yuv \
	-o ./gdc_output_1920x1080.yuv -w 1920 -h 1080 \
	-p process3 -C 10000 &
