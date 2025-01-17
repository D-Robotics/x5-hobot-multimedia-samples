#!/bin/bash

./qos.sh

./multi_pipe_crop_and_stitch -c "sensor=8 gdc=1" -c "sensor=8 gdc=1" -o hdmi -b -v -p -f 1 


