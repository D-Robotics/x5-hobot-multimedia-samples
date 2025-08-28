# 获取源码
glmark2  是开源代码，直接把源码放到EVB SDK中是无法编译的，需要按照如下步骤打上PATCH。

1. 进入上级目录，创建source
```sh
	cd ../ && mkdir source
```
2. 拉取代码（指定tag）

```sh
	cd source
	git clone https://github.com/glmark2/glmark2
	cd glmark2
	git checkout b565c52ff90043de14d3efdfe07b9f183fd2c43b
```

3. patch 文件赋值到源码目录
```sh
   cd src
   cp ../../glmark2/source-code-patch/*.patch .
   git am < *.patch
```

# 编译方法
安装命令会把glmakr2程序执行依赖的 data文件拷贝到 `chip_base_test/11_gpu_3d_test/glmark2`
```sh
	make
	make install
```

# 执行
1. 插上HDMI显示器 或者 DSI显示器（默认使用HDMI显示器，需要修改run.sh中的命令为 `./bin/glmark2 --data-path ./data -c DSI`）
把 `chip_base_test/11_gpu_3d_test/glmark2` 目录放到设备中, 执行如下命令：

```sh
	./run.sh
```
