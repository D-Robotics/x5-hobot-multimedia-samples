#!/bin/sh

if [ -L "/userdata/web" ];then
	echo "/userdata/web 软链接已经存在，删除重新链接"
	rm /userdata/web
fi

ln -s "$(pwd)" /userdata/web
export LD_LIBRARY_PATH=/userdata/web/pcre/lib:/userdata/web/fcgi/lib:"$LD_LIBRARY_PATH"
touch /userdata/web/sc_lighttpd/log/error.log

/userdata/web/sc_lighttpd/sbin/lighttpd -f /userdata/web/sc_lighttpd/config/lighttpd.conf -m /userdata/web/sc_lighttpd/lib/
