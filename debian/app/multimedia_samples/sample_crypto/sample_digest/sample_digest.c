
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

typedef const EVP_MD *(*EVP_GET_MD)(void);
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct sample_digest_test_item_s {
    char *digest_name;
    EVP_GET_MD get_digest;
}sample_digest_test_item_t;

static sample_digest_test_item_t digest_test_list[] = {
	/* MD5 */
	{"md5", EVP_md5},
	/* SHA-1 */
	{"sha1", EVP_sha1},
	/* SHA-2 */
	{"sha224", EVP_sha224},
	{"sha256", EVP_sha256},
	{"sha384", EVP_sha384},
	{"sha512", EVP_sha512},
	/* SHA-3 */
	{"sha3-224", EVP_sha3_224},
	{"sha3-256", EVP_sha3_256},
	{"sha3-384", EVP_sha3_384},
	{"sha3-512", EVP_sha3_512},
};

static struct option const long_options[] = {
	{"digest", required_argument, NULL, 's'},
	{"help", optional_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

static void print_help()
{
	int digest_test_item_num = ARRAY_SIZE(digest_test_list);
	int i;

	printf("Usage: sample_digest [Options] [value]\n");
	printf("Options:\n");
	printf("  -s <hash name>        Select Hash Name\n");
	printf("  -h                    Show this help message\n");

	printf("Available Hash:\n");
	for (i = 0; i < digest_test_item_num; i++){
		printf("\t %d. %s\n", i+1, digest_test_list[i].digest_name);
	}
}

void handleErrors()
{
	fprintf(stderr, "Error occurred\n");
	ERR_print_errors_fp(stderr);
	exit(1);
}

int main(int argc, char** argv)
{
	int opt_index = 0;
	int c = 0;
	int idx = 0;
	int digest_test_item_num = ARRAY_SIZE(digest_test_list);
	sample_digest_test_item_t *digest_test_item = NULL;
	const EVP_MD *digest = NULL;

	while((c = getopt_long(argc, argv, "s:h",
	long_options, &opt_index)) != -1) {
		switch (c)
		{
		case 's':
			for (idx = 0; idx < digest_test_item_num; idx++) {
				digest_test_item = &digest_test_list[idx];
				if (0 == strcmp(optarg, digest_test_item->digest_name)) {
					digest = digest_test_item->get_digest();
					break;
				}
			}
			if (idx >= digest_test_item_num) {
				printf("Unknow digest Name: %s\n", optarg);
				return 0;
			}
			break;
		case 'h':
		default:
			print_help();
			return 0;
		}
	}

	// Input plaintext
	unsigned char plain_text[128] = {0};
	unsigned char digest_text[128] = {0};
	unsigned int digest_len = 0;
	snprintf((char *)plain_text, 128, "Test message for digest [%s]", digest_test_item->digest_name);
	printf("plain_text: \n%s\n", plain_text);

	// 初始化 OpenSSL 库
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();

	// Initialize the EVP context
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	if (!ctx) {
		printf("EVP_MD_CTX_new() failed.\n");
		return 0;
	}

	// 初始化哈希算法
	if (1 != EVP_DigestInit_ex(ctx, digest, NULL)) {
		handleErrors();
	}

	// 更新哈希上下文，传入输入数据
	if (1 != EVP_DigestUpdate(ctx, plain_text, strlen((char *)plain_text))) {
		handleErrors();
	}

	// 完成哈希计算，获取哈希值
	if (1 != EVP_DigestFinal_ex(ctx, digest_text, &digest_len)) {
		handleErrors();
	}
	printf("[%s] hash is: \n", digest_test_item->digest_name);
	for (unsigned int i = 0; i < digest_len; i++) {
		printf("%02x", digest_text[i]);
	}
	printf("\n");

	EVP_MD_CTX_free(ctx);

	// 清理 OpenSSL
	EVP_cleanup();
	ERR_free_strings();

	return 0;
}


