#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

enum {
	MODE_NONE = 0,          // 无测试模式
	MODE_LOOPBACK = 1,      // 单路回环测试
	MODE_READ_ONLY = 2,     // 只读模式
	MODE_WRITE_ONLY = 4,    // 只写模式
	MODE_DUAL_LOOPBACK = 8  // 双路回环测试
};

// 定义调试模块
enum {
	DEBUG_MODULE_NONE = 0,        // 不打印调试日志
	DEBUG_MODULE_UART_SEND = 1,   // 输出写串口模块的日志
	DEBUG_MODULE_UART_RECV = 2,   // 输出读串口模块的日志
	DEBUG_MODULE_DATA_CHECK = 4   // 输出校验数据模块的日志
};

#define UT_FRAME_LEN 512
#define UT_BUFFER_SIZE (20 * 1024 * 1024)
static pthread_t recv_thread_id;
static pthread_t send_thread_id;
static pthread_t recv_check_thread_id;
static char send_buffer[UT_BUFFER_SIZE] = {0};
static char recv_buffer[UT_BUFFER_SIZE] = {0};
static uint32_t test_size = 1024;
static uint32_t baudrate = 115200;
static int32_t test_count = -1;
static char *uart_device = NULL;
static char *uart_device2 = NULL;
static int uart_test_mode = MODE_NONE;
static uint32_t verbose_enabled = DEBUG_MODULE_NONE;

static int g_uart_fd = -1;
static int g_uart2_fd = -1;
static uint64_t recv_total = 0;
static sem_t sem_check_data;
static sem_t sem_recv_started;
static sem_t sem_send_started;

static void dump_data(const char *title, uint8_t *data, uint32_t sum, uint32_t len) {
	printf("dump %s data:\n", title);
	for (uint32_t ii = 0; ii < len; ++ii) {
		if (ii % 32 == 0) {
			printf("0x%08x: ", sum + ii);
		}

		printf("%02x ", data[sum + ii]);

		if ((ii + 1) % 32 == 0) {
			printf("\n");
		}
	}
	printf("\n");
}

static void dump_recv_data(uint32_t sum, uint32_t len) {
	dump_data("receive", recv_buffer, sum, len);
}

static void dump_send_data(uint32_t sum, uint32_t len) {
	dump_data("send", send_buffer, sum, len);
}

static void set_baudrate(int fd, int nSpeed)
{
	struct termios newtio;
	tcgetattr(fd, &newtio);

	switch (nSpeed) {
		case 2400:
			cfsetispeed(&newtio, B2400);
			cfsetospeed(&newtio, B2400);
			break;

		case 4800:
			cfsetispeed(&newtio, B4800);
			cfsetospeed(&newtio, B4800);
			break;

		case 9600:
			cfsetispeed(&newtio, B9600);
			cfsetospeed(&newtio, B9600);
			break;

		case 19200:
			cfsetispeed(&newtio, B19200);
			cfsetospeed(&newtio, B19200);
			break;

		case 38400:
			cfsetispeed(&newtio, B38400);
			cfsetospeed(&newtio, B38400);
			break;

		case 57600:
			cfsetispeed(&newtio, B57600);
			cfsetospeed(&newtio, B57600);
			break;

		case 115200:
			cfsetispeed(&newtio, B115200);
			cfsetospeed(&newtio, B115200);
			break;
		case 230400:
			cfsetispeed(&newtio, B230400);
			cfsetospeed(&newtio, B230400);
			break;
		case 921600:
			cfsetispeed(&newtio, B921600);
			cfsetospeed(&newtio, B921600);
			break;
		case 1000000:
			cfsetispeed(&newtio, B1000000);
			cfsetospeed(&newtio, B1000000);
			break;

		case 1152000:
			cfsetispeed(&newtio, B1152000);
			cfsetospeed(&newtio, B1152000);
			break;
		case 1500000:
			cfsetispeed(&newtio, B1500000);
			cfsetospeed(&newtio, B1500000);
			break;
		case 2000000:
			cfsetispeed(&newtio, B2000000);
			cfsetospeed(&newtio, B2000000);
			break;
		case 2500000:
			cfsetispeed(&newtio, B2500000);
			cfsetospeed(&newtio, B2500000);
			break;
		case 3000000:
			cfsetispeed(&newtio, B3000000);
			cfsetospeed(&newtio, B3000000);
			break;
		case 3500000:
			cfsetispeed(&newtio, B3500000);
			cfsetospeed(&newtio, B3500000);
			break;

		case 4000000:
			cfsetispeed(&newtio, B4000000);
			cfsetospeed(&newtio, B4000000);
			break;

		default:
			printf("\tSorry, Unsupported baudrate, use previous baudrate!\n\n");
			break;
	}
	tcsetattr(fd,TCSANOW,&newtio);
}

static void set_termios(int fd)
{
	struct termios term;

	tcgetattr(fd, &term);
	term.c_cflag &= ~(CSIZE | CSTOPB | PARENB | INPCK);
	term.c_cflag |= (CS8 | CLOCAL | CREAD);
	term.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	term.c_oflag &= ~(OPOST | ONLCR | OCRNL);
	term.c_iflag &= ~(ICRNL |INLCR | IXON | IXOFF | IXANY);
	term.c_cc[VTIME] = 0;
	term.c_cc[VMIN] = 1;
	tcsetattr(fd, TCSAFLUSH, &term);
}

static void *uart_send_thread(void* arg)
{
	int uart_fd = *((int*)arg);
	uint32_t i = 0, j = 0;
	uint32_t exe_count = 0;
	int size = 0, sum = 0, len = 0;
	uint32_t *frame_value = NULL;
	uint32_t *frame_num = NULL;
	struct timeval start, end;

	printf("Starting send thread\n");

	// 初始化测试数据
	for (i = 0; i < test_size * 1024; i+=4) {
		if (i % UT_FRAME_LEN) {
			frame_value = (uint32_t *)&send_buffer[i];
			*frame_value = rand();
		}
	}

	// 初始化帧序号
	for (i = 0; i < test_size * 1024 / UT_FRAME_LEN; i++) {
		frame_num = (uint32_t *)&send_buffer[i * UT_FRAME_LEN];
		*frame_num = i;
		if (verbose_enabled & DEBUG_MODULE_UART_SEND) {
			printf("Frame ID: pos:0x%x, value:0x%x\n", i * UT_FRAME_LEN, *frame_num);
		}
	}

	if (uart_test_mode == MODE_LOOPBACK) {
		sem_wait(&sem_recv_started);  // Wait for recv_thread_id to start
		sleep(1);
	}

	i = 0;
	while (test_count == -1 || i < test_count) {
		usleep(500 * 1000); // wait 500ms

		printf("This is uart send test %d times\n", ++exe_count);
		if (verbose_enabled & DEBUG_MODULE_UART_SEND) {
			gettimeofday(&start, NULL);
		}
		sum = len = 0;
		for (j = 0; j <  test_size * 1024; j = j + UT_FRAME_LEN) {
			len = write(uart_fd, &send_buffer[j], UT_FRAME_LEN);
			if (len < UT_FRAME_LEN) {
				perror("Error reading from UART");
				return NULL;
			}

			if (verbose_enabled & DEBUG_MODULE_UART_SEND) {
				// 打印发送的数据
				dump_send_data(sum, len);
			}
			sum += len;
			size -= len;
		}

		if (verbose_enabled & DEBUG_MODULE_UART_SEND) {
			gettimeofday(&end, NULL);
			printf("start %ld sec, %ld usec, end %ld sec, %ld usec\n", start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec);
			double ts = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)) / 1000;
			printf("send %dKbytes,time:%fms, BPS:%f\n", test_size, ts, test_size * 1000 / (ts / 1000));
		}

		i++;
	}
	return NULL;
}

void* uart_recv_thread(void* arg)
{
	int uart_fd = *((int*)arg);
	int32_t i = 0;
	uint32_t exe_count = 0;
	int size = 0, sum = 0, len = 0;
	int len_frame = 0; /*use to get correct frame len*/
	struct timeval start, end;

	memset(recv_buffer, 0, sizeof(recv_buffer));

	printf("Starting receive thread\n");

	if (uart_test_mode == MODE_LOOPBACK) {
		sem_post(&sem_recv_started);
	}

	while (test_count == -1 || i < test_count) {
		sum = len = 0;
		printf("This is receive test %d times\n", ++exe_count);

		// 记录开始时间
		if (verbose_enabled & DEBUG_MODULE_UART_RECV) {
			gettimeofday(&start, NULL);
		}
		size = test_size * 1024;
		while (size > 0) {
			len = read(uart_fd, &recv_buffer[sum], UT_FRAME_LEN);
			if (len < 0) {
				perror("Error reading from UART");
				return NULL;
			}
			recv_total += len;
			len_frame += len;
			if (len_frame >= UT_FRAME_LEN) {
				len_frame -= UT_FRAME_LEN;
				// 每接收一个完整帧就释放信号量
				sem_post(&sem_check_data);
			}

			if (verbose_enabled & DEBUG_MODULE_UART_RECV) {
				// 打印接收的数据
				dump_recv_data(sum, len);
			}

			sum += len;
			size -= len;
		}

		// 记录结束时间，并输出接收速率
		if (verbose_enabled & DEBUG_MODULE_UART_RECV) {
			gettimeofday(&end, NULL);
			double ts = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
			printf("Receive %dKbytes, time: %fms, BPS: %f\n", test_size, ts, test_size * 1000 / (ts / 1000));
		}

		i++;
	}

	return NULL;
}

int32_t error_bit(const uint64_t *data1, const uint64_t *data2, int32_t len) {
	uint64_t xor_result = 0;
	int32_t bit_count = 0;

	for (int i = 0; i < len / 8; i++) {
		xor_result = data1[i] ^ data2[i];
		while (xor_result != 0) {
			xor_result &= (xor_result - 1);
			bit_count++;
		}
	}

	return bit_count;
}

void *check_recv_thread(void *arg) {
	size_t check_pos = 0;
	uint32_t *cur_frame = NULL;
	int32_t error_bit_cnt = 0;

	while (1) {
		sem_wait(&sem_check_data);
		// Check data
		cur_frame = (uint32_t *)&recv_buffer[check_pos];
		if (*cur_frame != check_pos / UT_FRAME_LEN) {
			printf("ERROR: may lost frame, curruent frame id is %d, expected frame id is %d, data position: 0x%zx\n",
				   *cur_frame, check_pos / UT_FRAME_LEN, check_pos);

			dump_recv_data(check_pos, UT_FRAME_LEN);
			dump_send_data(check_pos, UT_FRAME_LEN);

			error_bit_cnt = 0;
			error_bit_cnt = error_bit((const uint64_t *)&recv_buffer[check_pos],
									  (const uint64_t *)&send_buffer[check_pos],
									  UT_FRAME_LEN / 8);
			check_pos += UT_FRAME_LEN;

			printf("ERROR: Test total data count: 0x%lx, error bit count:%d\n", recv_total, error_bit_cnt);

			if (check_pos == test_size * 1024) {
				printf("ERROR: Frame head error\n");
			}
			continue;
		}

		error_bit_cnt = 0;
		error_bit_cnt = error_bit((const uint64_t *)&recv_buffer[check_pos],
								  (const uint64_t *)&send_buffer[check_pos],
								  UT_FRAME_LEN / 8);

		if (error_bit_cnt) {
			printf("ERROR: frame data error, curruent frame id is %d, expected frame id is %d, position: 0x%zx\n",
				   *cur_frame, check_pos / UT_FRAME_LEN, check_pos);

			dump_recv_data(check_pos, UT_FRAME_LEN);
			dump_send_data(check_pos, UT_FRAME_LEN);

			check_pos += UT_FRAME_LEN;

			printf("ERROR: Test total data count: 0x%lx, error bit count:%d\n", recv_total, error_bit_cnt);

			if (check_pos == test_size * 1024) {
				printf("ERROR: frame data error\n");
			}
			continue;
		}

		// Clear the received data
		memset(&recv_buffer[check_pos], 0, UT_FRAME_LEN);
		check_pos += UT_FRAME_LEN;

		if (check_pos == test_size * 1024) {
			check_pos = 0;
			if (verbose_enabled & DEBUG_MODULE_DATA_CHECK) {
				dump_recv_data(check_pos, UT_FRAME_LEN);
				dump_send_data(check_pos, UT_FRAME_LEN);
			}
			printf("Data verification successful. Received data matches sent data. Test total data count: 0x%lx\n", recv_total);
		}
	}

	return NULL;
}

int open_uart(const char* uart_device, speed_t baudrate) {
	int fd = open(uart_device, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror("Error: Unable to open UART device");
		return -1;
	}

	set_baudrate(fd, baudrate);
	set_termios(fd);

	return fd;
}

// 创建线程并等待线程结束的通用函数
static void create_thread(pthread_t* thread_id, void* thread_function, void* thread_arg, const char* thread_name) {
	int ret = 0;
	printf("Performing %s...\n", thread_name);

	ret = pthread_create(thread_id, NULL, thread_function, thread_arg);
	if (ret < 0) {
		printf("create %s thread failed\n", thread_name);
		exit(EXIT_FAILURE);
	}
}

// 单路回环测试功能
void perform_single_loopback_test() {
	g_uart_fd = open_uart(uart_device, baudrate);

	// 初始化 semaphores
	sem_init(&sem_check_data, 0, 0);
	sem_init(&sem_recv_started, 0, 0);

	create_thread(&recv_thread_id, uart_recv_thread, (void*)&g_uart_fd, "uart recv");
	create_thread(&send_thread_id, uart_send_thread, (void*)&g_uart_fd, "uart send");
	create_thread(&recv_check_thread_id, check_recv_thread, NULL, "data check");

	pthread_join(recv_check_thread_id, NULL);
	pthread_join(recv_thread_id, NULL);
	pthread_join(send_thread_id, NULL);
}

// 只读模式功能
void perform_read_only_test() {
	g_uart_fd = open_uart(uart_device, baudrate);
	create_thread(&recv_thread_id, uart_recv_thread, (void*)&g_uart_fd, "uart recv");

	pthread_join(recv_thread_id, NULL);
}

// 只写模式功能
void perform_write_only_test() {
	g_uart_fd = open_uart(uart_device, baudrate);
	create_thread(&send_thread_id, uart_send_thread, (void*)&g_uart_fd, "uart send");

	pthread_join(send_thread_id, NULL);
}

// 双路回环测试功能
void perform_dual_loopback_test() {
	g_uart_fd = open_uart(uart_device, baudrate);
	g_uart2_fd = open_uart(uart_device2, baudrate);

	// 初始化 semaphores
	sem_init(&sem_check_data, 0, 0);
	sem_init(&sem_recv_started, 0, 0);

	create_thread(&recv_thread_id, uart_recv_thread, (void*)&g_uart2_fd, "uart2 recv");
	create_thread(&send_thread_id, uart_send_thread, (void*)&g_uart_fd, "uart send");
	create_thread(&recv_check_thread_id, check_recv_thread, NULL, "data check");

	pthread_join(recv_check_thread_id, NULL);
	pthread_join(recv_thread_id, NULL);
	pthread_join(send_thread_id, NULL);
}

void check_options_validity() {
	if (uart_device == NULL) {
		fprintf(stderr, "Error: UART device must be specified using -d or --device.\n");
		exit(EXIT_FAILURE);
	}

	if (test_size < 0 || test_size > UT_BUFFER_SIZE) {
		fprintf(stderr, "Error: test_size must be between 0 and %d Bytes.\n", UT_BUFFER_SIZE);
		exit(EXIT_FAILURE);
	}

	if (baudrate < 0 || baudrate > 4000000) {
		printf("Error: Baudrate must be between 0 and baudrate(4M)\n");
		exit(EXIT_FAILURE);
	}

	if (test_count != -1 && test_count <= 0) {
		fprintf(stderr, "Error: Test count must be a positive integer or loop forever (-1).\n");
		exit(EXIT_FAILURE);
	}

	if ((uart_test_mode != MODE_NONE) && ((uart_test_mode & (uart_test_mode - 1)) != 0)) {
		// test_mode 必须是 2 的幂次方
		fprintf(stderr, "Error: Invalid combination of test modes. Multiple test modes are set. Please choose only one mode.\n");
		exit(EXIT_FAILURE);
	}

	if (uart_test_mode == MODE_DUAL_LOOPBACK && uart_device2 == NULL) {
		fprintf(stderr, "Error: Dual-loopback test requires specifying the second UART device using -u or --uart2.\n");
		exit(EXIT_FAILURE);
	}
}

static const char short_options[] = "s:c:b:d:lrwDu:V:h";
static const struct option long_options[] = {
	{"size", required_argument, NULL, 's'},
	{"baudrate", required_argument, NULL, 'b'},
	{"count", required_argument, NULL, 'c'},
	{"device", required_argument, NULL, 'd'},
	{"loopback", no_argument, NULL, 'l'},
	{"read-only", no_argument, NULL, 'r'},
	{"write-only", no_argument, NULL, 'w'},
	{"dual-loopback", no_argument, NULL, 'D'},
	{"uart2", required_argument, NULL, 'u'},
	{"verbose ", required_argument, NULL, 'V'},
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

void print_usage() {
	printf("Usage: uart_test [OPTIONS]\n");
	printf("Options:\n");
	printf("  -s, --size      : Specify the data size (KBytes) for the test, default is 1024KB, MAX is 20480KB(20MB)\n");
	printf("  -b, --baudrate  : Specify the baud rate for the UART, default is 115200\n");
	printf("  -c, --count     : Specify the number of test iterations, default is loop forever\n");
	printf("  -d, --device    : Specify the UART device\n");
	printf("  -l, --loopback  : Enable loopback test for UART (serial port)\n");
	printf("  -r, --read-only : Enable read-only mode for UART test\n");
	printf("  -w, --write-only: Enable write-only mode for UART test\n");
	printf("  -D, --dual-loopback : Enable dual-loopback test for UART (requires --uart2)\n");
	printf("  -u, --uart2     : Specify the second UART device (required for dual-loopback)\n");
	printf("  -V, --verbose   : Specify verbose level for additional print statements\n");
	printf("  -h, --help      : Display this help message\n");
	printf("\nExample: uart_test -l -s 1024 -b 115200 -c 10 -d /dev/ttyS1\n");
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int i = 0;
	int32_t cmd_parser_ret = 0;

	while ((cmd_parser_ret = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch (cmd_parser_ret) {
			case 's':
				test_size = atoi(optarg);
				break;
			case 'b':
				baudrate = atoi(optarg);
				break;
			case 'c':
				test_count = atoi(optarg);
				break;
			case 'd':
				uart_device = optarg;
				break;
			case 'l':
				uart_test_mode |= MODE_LOOPBACK;
				break;

			case 'r':
				uart_test_mode |= MODE_READ_ONLY;
				break;

			case 'w':
				uart_test_mode |= MODE_WRITE_ONLY;
				break;

			case 'D':
				uart_test_mode |= MODE_DUAL_LOOPBACK;
				break;
			case 'u':
				uart_device2 = optarg;
				break;
			case 'V':
				verbose_enabled  = atoi(optarg);
				break;
			case 'h':
				print_usage();
				exit(EXIT_SUCCESS);
			default:
				print_usage();
				exit(EXIT_FAILURE);
			}
	}

	check_options_validity();

	printf("Test uart device:%s\n", uart_device);
	printf("Test size: %d KBytes, baudrate: %d\n", test_size, baudrate);

	switch (uart_test_mode) {
		case MODE_LOOPBACK:
			perform_single_loopback_test();
			break;

		case MODE_READ_ONLY:
			perform_read_only_test();
			break;

		case MODE_WRITE_ONLY:
			perform_write_only_test();
			break;

		case MODE_DUAL_LOOPBACK:
			printf("Test uart device2:%s\n", uart_device2);
			perform_dual_loopback_test();
			break;

		default:
			fprintf(stderr, "Error: No valid test mode selected.\n");
			exit(EXIT_FAILURE);
	}

	return 0;
}
