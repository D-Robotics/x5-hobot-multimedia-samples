#!/bin/bash
script_dir=$(dirname "$(readlink -f "$0")")

cd $script_dir/sample_copy
./copy -m 1 -c 1920 -r 1080 -i 100 |grep "Case"

cd $script_dir/sample_format_convert
./format_convert -m 1 -c 1920 -r 1080 -i 100 |grep "Case"

cd $script_dir/sample_multi_source_alphablend
./multi_source_alphablend -m 1 -c 1920 -r 1080 -i 100 |grep "Case"

cd $script_dir/sample_resize
./resize -m 1 -c 1920 -r 1080 -i 100 |grep "Case"

cd $script_dir/sample_stitch
./stitch -m 1 -c 1920 -r 1080 -i 100 |grep "Case"

cd $script_dir/sample_alphablend
./alphablend -m 1 -c 1920 -r 1080 -i 100 |grep "Case"

cd $script_dir/sample_crop
./crop -m 1 -c 1920 -r 1080 -i 100 |grep "Case"

cd $script_dir/sample_rectangle_fill
./rectangle_fill -m 1 -c 1920 -r 1080 -i 100 |grep "Case"

cd $script_dir/sample_rotation
./rotation -m 1 -c 1920 -r 1080 -i 100 |grep "Case"
