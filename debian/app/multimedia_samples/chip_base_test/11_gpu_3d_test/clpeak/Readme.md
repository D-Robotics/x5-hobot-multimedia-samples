# 获取源码
clpeak 是开源代码，直接把源码放到EVB SDK中是无法编译的，需要按照如下步骤打上PATCH。

1. 进入上级目录，创建source
```sh
	cd ../ && mkdir source
```
2. 拉取代码（指定tag）

```sh
	cd source
	git clone --branch 1.1.4 --depth 1 https://github.com/krrishnarraj/clpeak.git
```

3. patch 文件赋值到源码目录
```sh
   cd clpeak
   cp ../../clpeak/source-code-patch/0001-Support-compilation-in-X5-EVB-SDK-environment.patch .
   git am < 0001-Support-compilation-in-X5-EVB-SDK-environment.patch
```

# 编译方法
```sh
	mkdir build
	cd build
	cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64_toolchain.cmake ..
	make
```

# 执行
把 `clpeak` 放到设备中, 执行如下命令：

```sh
	./clpeak
```
