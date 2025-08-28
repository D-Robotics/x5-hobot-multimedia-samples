
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/hmac.h>

typedef const EVP_MD *(*EVP_GET_MD)(void);
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct sample_hmac_test_item_s {
	char *hash_name;
	int key_size;
	EVP_GET_MD get_hash;
}sample_hmac_test_item_t;

static sample_hmac_test_item_t hmac_test_list[] = {
	/* MD5 */
	{"md5",     128,    EVP_md5},
	/* SHA-1 */
	{"sha1",    160,    EVP_sha1},
	/* SHA-2 */
	{"sha224",  224,    EVP_sha224},
	{"sha256",  256,    EVP_sha256},
	{"sha384",  384,    EVP_sha384},
	{"sha512",  512,    EVP_sha512},
	/* SHA-3 */
	{"sha3-224", 224,   EVP_sha3_224},
	{"sha3-256", 256,   EVP_sha3_256},
	{"sha3-384", 384,   EVP_sha3_384},
	{"sha3-512", 512,   EVP_sha3_512},
};

static struct option const long_options[] = {
	{"hash", required_argument, NULL, 's'},
	{"key", optional_argument, NULL, 'k'},
	{"help", optional_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

static void print_help()
{
	int hmac_test_item_num = ARRAY_SIZE(hmac_test_list);
	int i;

	printf("Usage: sample_hmac [Options] [value]\n");
	printf("Options:\n");
	printf("  -s <hash name>  Select hash Name\n");
	printf("  -k <key size>   Key Size\n");
	printf("  -h              Show this help message\n");

	printf("Available Hash:\n");
	printf("\tHash Name\t Recommend Key Size\n");
	for (i = 0; i < hmac_test_item_num; i++){
		printf("\t %d. %s  \t %d-bits\n", i+1, hmac_test_list[i].hash_name, hmac_test_list[i].key_size);
	}
}

void handleErrors() {
	fprintf(stderr, "Error occurred\n");
	ERR_print_errors_fp(stderr);
	exit(1);
}

int main(int argc, char** argv)
{
	unsigned int i = 0;
	int opt_index = 0;
	int c = 0;
	int idx = 0;
	int hmac_test_item_num = ARRAY_SIZE(hmac_test_list);
	sample_hmac_test_item_t *hmac_test_item = NULL;
	const EVP_MD *hash = NULL;
	int key_size = 0;

	while((c = getopt_long(argc, argv, "s:k:h",
	long_options, &opt_index)) != -1) {
		switch (c)
		{
		case 's':
			for (idx = 0; idx < hmac_test_item_num; idx++) {
				hmac_test_item = &hmac_test_list[idx];
				if (0 == strcmp(optarg, hmac_test_item->hash_name)) {
					hash = hmac_test_item->get_hash();
					break;
				}
			}
			if (idx >= hmac_test_item_num) {
				printf("Unknow hmac Name: %s\n", optarg);
				return 0;
			}
			break;
		case 'k':
			key_size = atoi(optarg);
			break;
		case 'h':
		default:
			print_help();
			return 0;
		}
	}

	if (hash == NULL || key_size <= 0) {
		print_help();
		return 0;
	}

	// Input plaintext
	unsigned char plain_text[128] = {0};
	unsigned char hmac_text[128] = {0};
	unsigned int hmac_len = 0;
	snprintf((char *)plain_text, 128, "Test message for Hmac: Hash[%s] Key[%d]", hmac_test_item->hash_name, key_size);
	printf("plain_text: \n%s\n", plain_text);

	// key
	unsigned char *key = NULL;
	key = malloc(key_size/8);
	if (key == NULL) {
		printf("Malloc for key error\n");
		return 0;
	}
	srand(time(NULL));
	for (i = 0; i < key_size/8; i++) {
		key[i] = '0' + (rand() % 10);
	}
	printf("\nKey is:\n%s\n", key);

	HMAC_CTX *ctx = HMAC_CTX_new(); // 创建 HMAC 上下文
	if (!ctx) {
		fprintf(stderr, "Failed to create HMAC context.\n");
		exit(1);
	}

	// 初始化 HMAC 上下文
	if (HMAC_Init_ex(ctx, key, key_size/8, hash, NULL) != 1) {
		fprintf(stderr, "HMAC initialization failed.\n");
		HMAC_CTX_free(ctx);
		exit(1);
	}

	// 更新 HMAC 计算
	if (HMAC_Update(ctx, plain_text, strlen((char *)plain_text)) != 1) {
		fprintf(stderr, "HMAC update failed.\n");
		HMAC_CTX_free(ctx);
		exit(1);
	}

	// 完成 HMAC 计算并获取结果
	if (HMAC_Final(ctx, hmac_text, &hmac_len) != 1) {
		fprintf(stderr, "HMAC finalization failed.\n");
		HMAC_CTX_free(ctx);
		exit(1);
	}

	printf("\nHMAC is: \n");
	for (i = 0; i < hmac_len; i++) {
		printf("%02x", hmac_text[i]);
	}
	printf("\n");

	HMAC_CTX_free(ctx);

	if (key)
		free(key);

	return 0;
}


