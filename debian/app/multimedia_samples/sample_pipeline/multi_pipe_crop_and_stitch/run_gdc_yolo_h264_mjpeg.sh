#!/bin/bash

./qos.sh

# 1 * GDC + 2 * 4K@30fps H264 (stream + file) + 1 * 4K@30fps MJPEG (stream + file) + yolov5@1fps
#./multi_pipe_crop_and_stitch -c "sensor=8 gdc=1 out264=1 outmjpeg=1 file=1" -c "sensor=8 out264=1 file=1" -b -v -p -f 1

# 1 * GDC + 2 * 4K@30fps H264 (stream) + 1 * 4K@30fps MJPEG (stream) + yolov5@1fps
./multi_pipe_crop_and_stitch -c "sensor=8 gdc=1 out264=1 outmjpeg=1" -c "sensor=8 out264=1" -b -v -p -f 1


