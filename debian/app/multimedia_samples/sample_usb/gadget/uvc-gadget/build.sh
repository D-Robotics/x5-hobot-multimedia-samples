#!/bin/bash

# When unexpected situations occur during script execution, exit immediately to avoid errors being ignored and incorrect final results
set -e

function do_all() {
	mkdir -p build
	cd build
	cmake .. --install-prefix=$PWD/_install

	make
	make install

	cd -

	if [ ! -z ${APP_DEPLOY_DIR} ]; then
		mkdir -p ${APP_DEPLOY_DIR}/platform_samples/sample_usb/uvc-gadget
		cp build/_install/bin/* ${APP_DEPLOY_DIR}/platform_samples/sample_usb/uvc-gadget/ -rf
		cp build/_install/lib/* ${APP_DEPLOY_DIR}/platform_samples/sample_usb/uvc-gadget/ -rf
	else
		echo "No app depoly dir, do nothing!!"
	fi
}

function do_clean()
{
	cd build

	make clean

	cd -

	rm build -rf
}

cmd=$1

if [ "$cmd" = "clean" ]
then
	echo "start to clean"
	do_clean
else
	# start to build
	echo "start to compile gc820 testcase:"
	do_all
fi
