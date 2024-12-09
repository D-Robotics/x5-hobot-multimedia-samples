#!/bin/bash

./qos.sh

./multi_pipe_crop_and_stitch -c "sensor=8 gdc=1" -c "sensor=8 gdc=1" -o file -b -v -p -f 1


