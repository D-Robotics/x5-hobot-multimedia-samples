#!/bin/bash

#insmod display module related drivers
modprobe panel-jc-050hd134
modprobe galcore
modprobe vio_n2d
modprobe lontium_lt8618
modprobe vs-x5-syscon-bridge
modprobe vs_drm

#run glmark2
./bin/glmark2 --data-path ./data
