#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include "sysinfopro.h"
#include "boardinfo.h"

#define BUF_LEN 80
#define MAX_LINE 256

void get_hardware_info() {
	FILE *model_file;
	FILE *board_id_file;
	char model[MAX_LINE];
	char board_id[MAX_LINE];

	// 读取设备型号信息
	model_file = fopen("/proc/device-tree/model", "r");
	if (model_file == NULL) {
		perror("Failed to open /proc/device-tree/model");
		return;
	}
	// 假设设备型号不会超过 255 字符
	if (fgets(model, sizeof(model), model_file) == NULL) {
		perror("Failed to read device model");
		fclose(model_file);
		return;
	}
	fclose(model_file);

	// 移除换行符（如果有）
	model[strcspn(model, "\n\r")] = '\0';

	// 读取板卡 ID 信息
	board_id_file = fopen("/sys/class/socinfo/board_id", "r");
	if (board_id_file == NULL) {
		perror("Failed to open /sys/class/socinfo/board_id");
		return;
	}
	// 假设板卡 ID 不会超过 255 字符
	if (fgets(board_id, sizeof(board_id), board_id_file) == NULL) {
		perror("Failed to read board ID");
		fclose(board_id_file);
		return;
	}
	fclose(board_id_file);

	// 移除板卡 ID 中的换行符（如果有）
	board_id[strcspn(board_id, "\n\r")] = '\0';

	// 输出硬件信息
	printf("[Hardware Model]:\n");
	printf("\t%s (Board Id = %s)\n\n", model, board_id);
}

// 检测 Flash 类型并返回设备路径
const char* detect_flash_device() {
	char buffer[1024];
	FILE *fp;

	// 使用 findmnt 命令检查根目录的挂载源
	fp = popen("findmnt -n -o SOURCE /", "r");
	if (fp == NULL) {
		perror("Failed to run findmnt command");
		return NULL;
	}

	if (fgets(buffer, sizeof(buffer), fp) != NULL) {
		buffer[strcspn(buffer, "\n")] = '\0'; // 去掉字符串末尾的换行符
		pclose(fp);

		// printf("Detected flash device: %s\n", buffer);

		// 判断设备类型
		if (strncmp(buffer, "/dev/mmcblk", 8) == 0) {
			return "/dev/mmcblk0";
		} else if (strncmp(buffer, "/dev/mtd", 8) == 0) {
			return "/dev/mtd0";
		} else {
			printf("Unknown flash device type: %s\n", buffer);
			return NULL; // 未知设备类型
		}
	} else {
		pclose(fp);
		printf("No valid flash device found\n");
		return NULL; // 没有找到挂载源
	}
}

void get_os_version() {
	FILE *fp;
	char buffer[1024];
	char command[128];

	// 获取 SDK OS 版本信息
	fp = popen("tr -d '\\n\\r\\0' < /etc/version", "r");
	if (fp == NULL) {
		perror("Failed to run command for SDK OS Version");
		return;
	}

	printf("[SDK Version]:\n");
	if (fgets(buffer, sizeof(buffer), fp) != NULL) {
		printf("\t%s", buffer); // 打印 SDK OS 版本
	}
	pclose(fp);
	printf("\n");

	// 获取内核版本信息
	fp = popen("uname -a | tr -d '\\0'", "r");
	if (fp == NULL) {
		perror("Failed to run command for Kernel Version");
		return;
	}

	printf("\n[Kernel Version]:\n");
	if (fgets(buffer, sizeof(buffer), fp) != NULL) {
		printf("\t%s", buffer); // 打印内核版本
	}
	pclose(fp);
	printf("\n");

	// 获取 Uboot 版本信息
	const char *device = detect_flash_device();

	if (!device) {
		perror("[Error] No supported flash device found\n");
		return;
	}
	// 使用 "grep -m 1" 限制查找到第一个匹配项后立即退出
	snprintf(command, sizeof(command), "strings %s | grep -m 1 -E 'U-Boot [0-9]{4}\\.[0-9]{2}.*\\('", device);
	fp = popen(command, "r");
	if (fp == NULL) {
			perror("Failed to run command for Miniboot Version");
			return;
	}

	printf("[Uboot Version]:\n");
	if (fgets(buffer, sizeof(buffer), fp) != NULL) {
		printf("\t%s", buffer); // 打印 Miniboot 版本
	}
	pclose(fp);
	printf("\n");
}

void get_bpu_hw_io_version(void) {
	FILE *fp;
	char buffer[MAX_LINE];
	char *version_start;
	char version[BUF_LEN];  // 用来保存版本号
	const char *command = "dmesg | grep 'bpu-core: hw-io:'";  // 当前执行的命令

	// 执行 dmesg 命令并将输出重定向到 fp
	fp = popen(command, "r");
	if (fp == NULL) {
		// 打印执行失败的命令和错误信息
		perror("Failed to run command.");
		printf("Command attempted: %s\n", command);
		return;
	}

	// 读取 dmesg 输出的每一行
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		// 查找 "bpu-core: hw-io:" 后面的内容
		version_start = strstr(buffer, "bpu-core: hw-io:");
		if (version_start) {
			// 跳过 "bpu-core: hw-io: git commit: " 部分
			version_start += strlen("bpu-core: hw-io: git commit: ");

			// 提取版本号并存入 version 中
			snprintf(version, sizeof(version), "%s", version_start);
			version[strcspn(version, "\n")] = '\0';  // 去除末尾的换行符

			pclose(fp);

			// 打印版本号
			printf("\n[Bpu HW_IO Git Commit Hash]:\n\t%s\n\n", version);
			return;  // 找到版本号后返回
		}
	}

	pclose(fp);

	// 如果没有找到版本号，打印 "Unknown"
	printf("\n[Bpu HW_IO Git Commit Hash]:\n\tUnknown\n\n");
}


void show_hrut_status() {
	FILE *fp;
	char buffer[1024];

	// 执行命令并获取输出
	fp = popen("bash -c \"hrut_somstatus | tr -d '\\0' | sed 's/^/\\t/'\"", "r");
	if (fp == NULL) {
		perror("Failed to run command");
		return;
	}

	// 打印输出的标题
	printf("[CPU DDR GPU And BPU Status]:\n");

	// 逐行读取命令输出并打印
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		printf("%s", buffer); // 打印每一行
	}

	// 关闭文件流
	pclose(fp);
}

void get_memory_info() {
	FILE *fp;
	char line[MAX_LINE];

	// 打开 /proc/meminfo 文件
	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		perror("Failed to open /proc/meminfo");
		return;
	}

	unsigned long total_memory = 0;
	unsigned long free_memory = 0;
	unsigned long available_memory = 0;
	unsigned long buffers = 0;
	unsigned long cached = 0;

	// 读取文件中的每一行，获取需要的内存信息
	while (fgets(line, sizeof(line), fp)) {
		if (sscanf(line, "MemTotal: %lu kB", &total_memory) == 1) {
			total_memory *= 1024; // 转换为字节
		}
		if (sscanf(line, "MemFree: %lu kB", &free_memory) == 1) {
			free_memory *= 1024; // 转换为字节
		}
		if (sscanf(line, "MemAvailable: %lu kB", &available_memory) == 1) {
			available_memory *= 1024; // 转换为字节
		}
		if (sscanf(line, "Buffers: %lu kB", &buffers) == 1) {
			buffers *= 1024; // 转换为字节
		}
		if (sscanf(line, "Cached: %lu kB", &cached) == 1) {
			cached *= 1024; // 转换为字节
		}
	}

	fclose(fp);

	// 计算已用内存 ( 已用内存 = 总内存 - 空闲内存 - 缓存 - 缓冲 )
	unsigned long used_memory = total_memory - free_memory - buffers - cached;

	// 打印内存信息
	printf("\n[Memory Info]:\n");
	printf("\t[Total Memory]:\t\t%.2f GB\n", total_memory / (1024.0 * 1024 * 1024));
	printf("\t[Used Memory]:\t\t%.2f GB\n", used_memory / (1024.0 * 1024 * 1024));
	printf("\t[Free Memory]:\t\t%.2f GB\n\n", free_memory / (1024.0 * 1024 * 1024));
	printf("\t[NOTE] What is displayed is the memory available to the system,\n");
	printf("\twhich is the actual physical memory capacity minus ION and system reserved memory.\n");
	printf("\t(The content is consistent with \"free -h\")\n");
}

void get_ion_cma_memory_size() {
	FILE *file;
	char line[MAX_LINE];
	unsigned long ion_cma_size_bytes = 0;

	// 打开文件 "/sys/kernel/debug/ion/heaps/ion_cma"
	file = fopen("/sys/kernel/debug/ion/heaps/ion_cma", "r");
	if (file == NULL) {
		perror("Error opening file");
		return;
	}

	// 逐行读取文件内容
	while (fgets(line, sizeof(line), file) != NULL) {
		// 查找包含 "ion_cma  heap total size" 的行
		if (strstr(line, "ion_cma  heap total size") != NULL) {
			// 提取数字部分，假设该行的内存大小紧跟在文本后
			if (sscanf(line, " ion_cma  heap total size        %lu", &ion_cma_size_bytes) == 1) {
				break;
			}
		}
	}

	// 如果找到内存大小，则将其转换为 GB（浮动小数点）
	if (ion_cma_size_bytes > 0) {
		float ion_cma_size_gb = (float)ion_cma_size_bytes / (1024 * 1024 * 1024);
		printf("\t[ION CMA Memory Size]:\t\t%.2f GB\n", ion_cma_size_gb);
	} else {
		printf("\tION CMA Memory Size not found.\n");
	}

	// 关闭文件
	fclose(file);
}

void get_ion_carveout_memory_size() {
	FILE *file;
	char line[MAX_LINE];
	unsigned long ion_carveout_size_bytes = 0;

	// 打开文件 "/sys/kernel/debug/ion/heaps/carveout"
	file = fopen("/sys/kernel/debug/ion/heaps/carveout", "r");
	if (file == NULL) {
		perror("Error opening file");
		return;
	}

	// 逐行读取文件内容
	while (fgets(line, sizeof(line), file) != NULL) {
		// 查找包含 "carveout  heap total size" 的行
		if (strstr(line, "carveout  heap total size") != NULL) {
			// 提取数字部分，假设该行的内存大小紧跟在文本后
			if (sscanf(line, " carveout  heap total size        %lu", &ion_carveout_size_bytes) == 1) {
				break;
			}
		}
	}

	// 如果找到内存大小，则将其转换为 GB（浮动小数点）
	if (ion_carveout_size_bytes > 0) {
		float ion_carveout_size_gb = (float)ion_carveout_size_bytes / (1024 * 1024 * 1024);
		printf("\t[ION Carveout Memory Size]:\t%.2f GB\n", ion_carveout_size_gb);
	} else {
		printf("\tION Carveout Memory Size not found.\n");
	}

	// 关闭文件
	fclose(file);
}

void get_ion_reserved_memory_size() {
	FILE *file;
	char line[MAX_LINE];
	unsigned long ion_reserved_size_bytes = 0;

	// 打开文件 "/sys/kernel/debug/ion/heaps/cma_reserved"
	file = fopen("/sys/kernel/debug/ion/heaps/cma_reserved", "r");
	if (file == NULL) {
		perror("Error opening file");
		return;
	}

	// 逐行读取文件内容
	while (fgets(line, sizeof(line), file) != NULL) {
		// 查找包含 "cma_reserved  heap total size" 的行
		if (strstr(line, "cma_reserved  heap total size") != NULL) {
			// 提取数字部分，假设该行的内存大小紧跟在文本后
			if (sscanf(line, "    cma_reserved  heap total size       %lu", &ion_reserved_size_bytes) == 1) {
				break;
			}
		}
	}

	// 如果找到内存大小，则将其转换为 GB（浮动小数点）
	if (ion_reserved_size_bytes > 0) {
		float ion_reserved_size_gb = (float)ion_reserved_size_bytes / (1024 * 1024 * 1024);
		printf("\t[ION Reserved Memory Size]:\t%.2f GB\n", ion_reserved_size_gb);
	} else {
		printf("ION Reserved Memory Size not found.\n");
	}

	// 关闭文件
	fclose(file);
}

void get_ion_memory_size() {
	printf("\n[ION Memory Info]:\n");
	get_ion_cma_memory_size();
	get_ion_carveout_memory_size();
	get_ion_reserved_memory_size();
	printf("\n");
}

void get_board_info() {
	// Define an array containing all keys
	char* boardinfo_key[] = {
		"soc_gen",
		"soc_name",
		"hw_name",
		"board_version",
		"hw_info",
		"bootdevice_name",
		"soc_uid",
		"ddr_type",
		"ddr_size",
		"ddr_vendor",
		"ddr_freq",
		"board_id",
		"bak_slot"
	};

	// Buffer is used to store the value of each key
	char buffer[BUF_LEN];
	int ret;

	printf("[Board Info]:\n");
	// Traverse the array and call hb_get_boardinfo to print the value of each key
	for (int i = 0; i < sizeof(boardinfo_key) / sizeof(boardinfo_key[0]); i++) {
		ret = hb_get_boardinfo(boardinfo_key[i], buffer, sizeof(buffer));

		if (ret == 0) {
			printf("\t%s: %s\n", boardinfo_key[i], buffer);
		} else {
			printf("%s: Failed to get value\n", boardinfo_key[i]);
		}
	}
	printf("\n");
}

void get_hbre_version() {
	// licam
	const char* libcam_version_info = hb_libcam_get_version_info();
	// libvpf
	const char* libvpf_version_info = hb_libvpf_get_version_info();
	// libdsp
	const char* libdsp_version_info = hb_libdsp_get_version_info();
	// libboardinfo
	const char* libboardinfo_version_info = hb_libboardinfo_get_version_info();
	// libefuse
	const char* libefuse_version_info = hb_libefuse_get_version_info();
	// libhbmem
	const char* libhbmem_version_info = hb_libhbmem_get_version_info();
	// libhbipcfhal
	const char* libhbipcfhal_version_info = hb_libhbipcfhal_get_version_info();
	// libalog
	const char* libalog_version_info = hb_libalog_get_version_info();
	// libmultimedia
	const char* libmultimedia_version_info = hb_libmultimedia_get_version_info();
	// libte600_engine
	const char* libte600_engine_version_info = hb_libte600_engine_get_version_info();
	// libupdate
	const char* libupdate_version_info = hb_libupdate_get_version_info();
	// libpowerctl
	const char* libpowerctl_version_info = hb_libpowerctl_get_version_info();
	// libsecure_storage
	const char* libsecure_storage_version_info = hb_libsecure_storage_get_version_info();
	// libhbplayer
	const char* libhbplayer_version_info = hb_libhbplayer_get_version_info();
	// libdnn
	const char* libdnn_version_info = hbDNNGetVersion();
	// libNano2D libNano2Dutil
	const char* libNano2D_version_info = n2d_get_version_info();
	// gc8000l lib version
	const char* gc8000l_version_info = gc8000l_get_version_info();
	// x5_camsys libs
	const char* x5_camsys_libs_version_info = hb_libhal_get_version_info();

	printf("[HBRE lib version]:\n");
	printf("\t%-30s %s\n", "libcam", libcam_version_info);
	printf("\t%-30s %s\n", "libvpf", libvpf_version_info);
	printf("\t%-30s %s\n", "libdsp", libdsp_version_info);
	printf("\t%-30s %s\n", "libboardinfo", libboardinfo_version_info);
	printf("\t%-30s %s\n", "libefuse", libefuse_version_info);
	printf("\t%-30s %s\n", "libhbmem", libhbmem_version_info);
	printf("\t%-30s %s\n", "libhbipcfhal", libhbipcfhal_version_info);
	printf("\t%-30s %s\n", "libalog", libalog_version_info);
	printf("\t%-30s %s\n", "libmultimedia", libmultimedia_version_info);
	printf("\t%-30s %s\n", "libte600_engine", libte600_engine_version_info);
	printf("\t%-30s %s\n", "libupdate", libupdate_version_info);
	printf("\t%-30s %s\n", "libpowerctl", libpowerctl_version_info);
	printf("\t%-30s %s\n", "libsecure_storage", libsecure_storage_version_info);
	printf("\t%-30s %s\n", "libhbplayer", libhbplayer_version_info);
	printf("\t%-30s %s\n", "libdnn", libdnn_version_info);
	printf("\t%-30s %s\n", "gc8000l", gc8000l_version_info);
	printf("\t%-30s %s\n", "x5_camsys", x5_camsys_libs_version_info);
	printf("\t%-30s %s\n\n", "libNano2D/libNano2Dutil", libNano2D_version_info);
}

void get_kernel_modules() {
	FILE *fp;
	char buffer[1024];

	// 使用 popen 执行 "lsmod" 命令并读取输出
	fp = popen("lsmod", "r");
	if (fp == NULL) {
		perror("Failed to run lsmod command");
		return;
	}

	// 输出标题
	printf("[Kernel Module List]:\n");

	// 逐行读取 lsmod 输出，并打印每一行内容（前面加制表符缩进）
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		// 使用循环逐个字符判断并去除 NULL 字符（如果有的话）
		for (int i = 0; buffer[i] != '\0'; i++) {
			if (buffer[i] == '\0') {
				buffer[i] = ' '; // 将 NULL 字符替换为空格
			}
		}

		// 每行前加上一个制表符并打印
		printf("\t%s", buffer);
	}

	pclose(fp);
}

// 定义选项对应的函数指针类型
typedef void (*OptionHandler)(int argc, char *argv[]);

// 选项映射表
typedef struct {
	char option;           // 选项字符（例如 'a'）
	OptionHandler handler; // 对应的处理函数
	const char* description; // 选项描述
} OptionMap;

// 选项处理函数
void handle_all() {
	get_hardware_info();
	get_os_version();
	get_bpu_hw_io_version();
	show_hrut_status();
	get_memory_info();
	get_ion_memory_size();
	get_board_info();
	get_hbre_version();
	get_kernel_modules();
}

void handle_memory() {
	get_memory_info();
	get_ion_memory_size();
}

void handle_os() {
	get_os_version();
	get_bpu_hw_io_version();
}

void print_usage(int argc, char *argv[]);

OptionMap options[] = {
	{'a', handle_all, "Show all information"},
	{'m', handle_memory, "Show memory information, include ION memory"},
	{'i', get_hardware_info, "Show hardware information"},
	{'o', handle_os, "Show OS version"},
	{'s', show_hrut_status, "Show temperature and frequency of CPU DDR GPU and BPU"},
	{'b', get_board_info, "Show basic information of board terminal"},
	{'l', get_hbre_version, "Show hbre library version"},
	{'k', get_kernel_modules, "Show currently loaded kernel modules"},
	{'h', (OptionHandler)print_usage, "Show this help message"},
};

void print_usage(int argc, char *argv[]) {
	// 检查 argv[0] 是否以 "./" 开头
	if (strncmp(argv[0], "./", 2) == 0) {
		// 去掉 "./" 部分，将指针向后移动两位
		argv[0] += 2;
	}
	printf("\n%s - display system information and hardware details.\n\n", argv[0]);
	printf("Usage: %s [options]\n\n", argv[0]);
	printf("Options can be combined. For example:\n");
	printf("\t ./%s -m -i -o\n\n", argv[0]);
	printf("Options:\n");
	for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
		printf("\t-%c\t%s\n", options[i].option, options[i].description);
	}
}

OptionHandler find_handler(char option) {
	for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
		if (options[i].option == option) {
			return options[i].handler;
		}
	}
	return NULL;
}

int main(int argc, char* argv[]) {
	// 如果没有参数，默认执行 -a
	if (argc == 1) {
		handle_all();
		return 0;
	}

	// 遍历所有参数
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || strlen(argv[i]) != 2) {
			printf("Invalid option: %s\n", argv[i]);
			print_usage(argc, argv);
			return 1;
		}

		char option = argv[i][1];
		OptionHandler handler = find_handler(option);

		if (handler) {
			handler(argc, argv);
		} else {
			printf("Invalid option: -%c\n", option);
			print_usage(argc, argv);
			return 1;
		}
	}

	return 0;
}
