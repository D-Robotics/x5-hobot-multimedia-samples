#!/bin/bash
script_dir=$(dirname "$(readlink -f "$0")")
cd $script_dir/bin

./copy -m 1 -c 3840 -r 2160 -i 100 |grep "Case"
./format_convert -m 1 -c 3840 -r 2160 -i 100 |grep "Case"
./multi_source_alphablend -m 1 -c 3840 -r 2160 -i 100 |grep "Case"
./resize -m 1 -c 1920 -r 1080 -i 100 |grep "Case"
./stitch -m 1 -c 3840 -r 2160 -i 100 |grep "Case"
./alphablend -m 1 -c 3840 -r 2160 -i 100 |grep "Case"
./crop -m 1 -c 3840 -r 2160 -i 100 |grep "Case"
./rectangle_fill -m 1 -c 3840 -r 2160 -i 100 |grep "Case"
./rotation -m 1 -c 3840 -r 2160 -i 100 |grep "Case"
