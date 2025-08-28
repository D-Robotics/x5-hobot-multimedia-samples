# sample_trustzone参考示例
sample_trustzone 分别实现了 CA 和 TA 示例代码。
- CA 代码位于 sample_ca 目录，实现了在 REE 环境下调用 TA
- TA 代码位于 sample_ta 目录，实现了在 TEE 环境下完成命令操作

注意事项
- sample_ca 可直接在 platform_samples 环境下编译。
- sample_ta 需拷贝至 miniboot 仓库下编译 `cp app/samples/platform_samples/sample_trustzone/sample_ta  miniboot/optee/hobot_tee_devkit/ta/customer/ -rf`
具体操作方法，请参考 SDK 文档《sample_trustzone 使用说明》
