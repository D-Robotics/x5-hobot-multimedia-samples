##使用说明##
1.烧写debug版本的固件

2.把测试程序和测试脚本传入板子中的/userdata分区(在电脑的终端执行)
adb push autotestver1.tar /userdata


3.解压autotestver1.tar
tar -xvf autotestver1.tar

4.修改/userdata/chip_base_test/config下的config.ini配置文件配置每个模块压测的时间

5.重启板子后，自动测试
测试结果保存在 /userdata/chip_base_test/log目录下




##Notice##
#如果有需要，可以自己添加其他模块参照实例代码/userdata/chip_base_test/03_nand_flash/nand_test.sh

```bash
#!/bin/sh
source ../Config/ini_parser_functions.sh

# 定义INI文件路径
ini_file="../Config/config.ini"

# 定义要解析的section和键值
target_section="Flash"
target_key="Ftime"

# 执行解析并将输出保存到变量
parsed_value=$(parse_ini_section "$target_section" "$target_key" "$ini_file")
```

