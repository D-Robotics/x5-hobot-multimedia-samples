
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

typedef const EVP_CIPHER *(*EVP_GET_CIPHER)(void);
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct sample_cipher_test_item_s {
	char *cipher_name;
	EVP_GET_CIPHER key64_get_cipher;
	EVP_GET_CIPHER key128_get_cipher;
	EVP_GET_CIPHER key192_get_cipher;
	EVP_GET_CIPHER key256_get_cipher;
}sample_cipher_test_item_t;

static sample_cipher_test_item_t cipher_test_list[] = {
	/*Cipher Name,  64bit,          128bit              192bit              256bit */
	/* aes */
	{"aes_ecb",     NULL,   EVP_aes_128_ecb,    EVP_aes_192_ecb,    EVP_aes_256_ecb},
	{"aes_cbc",     NULL,   EVP_aes_128_cbc,    EVP_aes_192_cbc,    EVP_aes_256_cbc},
	{"aes_cfb",     NULL,   EVP_aes_128_cfb,    EVP_aes_192_cfb,    EVP_aes_256_cfb},
	{"aes_ofb",     NULL,   EVP_aes_128_ofb,    EVP_aes_192_ofb,    EVP_aes_256_ofb},
	{"aes_ctr",     NULL,   EVP_aes_128_ctr,    EVP_aes_192_ctr,    EVP_aes_256_ctr},
	/* sm4 */
	{"sm4_ecb",     NULL,   EVP_sm4_ecb,    NULL,    NULL},
	{"sm4_cbc",     NULL,   EVP_sm4_cbc,    NULL,    NULL},
	{"sm4_cfb",     NULL,   EVP_sm4_cfb,    NULL,    NULL},
	{"sm4_ofb",     NULL,   EVP_sm4_ofb,    NULL,    NULL},
	{"sm4_ctr",     NULL,   EVP_sm4_ctr,    NULL,    NULL},
	/* des */
	{"des_ecb",     EVP_des_ecb,   NULL,    NULL,    NULL},
	{"des_cbc",     EVP_des_cbc,   NULL,    NULL,    NULL},
	{"des_ofb",     EVP_des_ofb,   NULL,    NULL,    NULL},
};

static struct option const long_options[] = {
	{"cipher", required_argument, NULL, 's'},
	{"key", optional_argument, NULL, 'k'},
	{"help", optional_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

static void print_help()
{
	int cipher_test_item_num = ARRAY_SIZE(cipher_test_list);
	int i;

	printf("Usage: sample_cipher [Options] [value]\n");
	printf("Options:\n");
	printf("  -s <cipher name>      Select Cipher Name\n");
	printf("  -k <key size>         Key Size [64/128/192/256]\n");
	printf("  -h                    Show this help message\n");

	printf("Available Cipher:\n");
	printf("\tCipher Name\tRecommend Key Size\n");
	for (i = 0; i < cipher_test_item_num; i++){
		printf("\t %d. %s\t", i+1, cipher_test_list[i].cipher_name);
		printf("%s%s%s%s", \
			((cipher_test_list[i].key64_get_cipher)?"64/":""),
			((cipher_test_list[i].key128_get_cipher)?"128/":""),
			((cipher_test_list[i].key192_get_cipher)?"192/":""),
			((cipher_test_list[i].key256_get_cipher)?"256":""));
		printf("-bit\n");
	}
}

static const EVP_CIPHER *test_item_get_cipher(sample_cipher_test_item_t *cipher_test_item, int key_size)
{
	const EVP_CIPHER *cipher = NULL;
	if (key_size == 64) {
		if (NULL != cipher_test_item->key64_get_cipher) {
			cipher = cipher_test_item->key64_get_cipher();
		}
		else {
			printf("Cipher [%s] Not Support Key Size[%d]\n", cipher_test_item->cipher_name, key_size);
			return NULL;
		}
	}
	else if (key_size == 128) {
		if (NULL != cipher_test_item->key128_get_cipher) {
			cipher = cipher_test_item->key128_get_cipher();
		}
		else {
			printf("Cipher [%s] Not Support Key Size[%d]\n", cipher_test_item->cipher_name, key_size);
			return NULL;
		}
	}
	else if (key_size == 192) {
		if (NULL != cipher_test_item->key192_get_cipher) {
			cipher = cipher_test_item->key192_get_cipher();
		}
		else {
			printf("Cipher [%s] Not Support Key Size[%d]\n", cipher_test_item->cipher_name, key_size);
			return NULL;
		}
	}
	else if (key_size == 256) {
		if (NULL != cipher_test_item->key256_get_cipher) {
			cipher = cipher_test_item->key256_get_cipher();
		}
		else {
			printf("Cipher [%s] Not Support Key Size[%d]\n", cipher_test_item->cipher_name, key_size);
			return NULL;
		}
	}
	else {
		printf("Unknow Key Size: %d\n", key_size);
		return NULL;
	}

	return cipher;
}

int main(int argc, char** argv)
{
	int opt_index = 0;
	int c = 0;
	int idx = 0;
	int cipher_test_item_num = ARRAY_SIZE(cipher_test_list);
	sample_cipher_test_item_t *cipher_test_item = NULL;
	const EVP_CIPHER *cipher = NULL;
	int key_size = 0;
	int len, ciphertext_len, decryptedtext_len;

	while((c = getopt_long(argc, argv, "s:k:h",
	long_options, &opt_index)) != -1) {
		switch (c)
		{
		case 's':
			for (idx = 0; idx < cipher_test_item_num; idx++) {
				cipher_test_item = &cipher_test_list[idx];
				if (0 == strcmp(optarg, cipher_test_item->cipher_name)) {
					break;
				}
			}
			if (idx >= cipher_test_item_num) {
				printf("Unknow Cipher Name: %s\n", optarg);
				return 0;
			}
			break;
		case 'k':
			key_size = atoi(optarg);
			cipher = test_item_get_cipher(cipher_test_item, key_size);
			if (cipher == NULL) {
				return 0;
			}
			break;
		case 'h':
		default:
			print_help();
			return 0;
		}
	}

	if (NULL == cipher || key_size <= 0) {
		print_help();
		return 0;
	}

	// AES key
	unsigned char *key = NULL;
	key = malloc(key_size/8);
	if (key == NULL) {
		printf("Malloc for key error\n");
		return 0;
	}

	if (RAND_bytes(key, key_size/8) != 1) {
		free(key);
		printf("Failed to generate random numbers key\n");
		return 0;
	}

	// 128-bit initialization vector (IV)
	unsigned char iv[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

	// Input plaintext
	unsigned char plaintext[128] = {0};
	unsigned char ciphertext[128] = {0};
	unsigned char decryptedtext[128] = {0};
	snprintf((char *)plaintext, 128, "Test message for encryption, Cipher[%s], KeySize[%d]", cipher_test_item->cipher_name, key_size);

	// Initialize the EVP context
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (!ctx) {
		printf("EVP_CIPHER_CTX_new() failed.\n");
		return 0;
	}

	// Encryption
	if (1 != EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv)) {
		printf("EVP_EncryptInit_ex failed\n");
		return 0;
	}

	if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, strlen((char *)plaintext))) {
		printf("EVP_EncryptUpdate failed\n");
		return 0;
	}
	ciphertext_len = len;

	if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
		printf("EVP_EncryptFinal_ex failed\n");
		return 0;
	}
	ciphertext_len += len;

	printf("Encrypted text (hex): ");
	for (int i = 0; i < ciphertext_len; i++) {
		printf("%02x", ciphertext[i]);
	}
	printf("\n");

	// Decryption
	EVP_CIPHER_CTX_reset(ctx);
	if (1 != EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv)) {
		printf("EVP_DecryptInit_ex failed\n");
		return 0;
	}

	if (1 != EVP_DecryptUpdate(ctx, decryptedtext, &len, ciphertext, ciphertext_len)) {
		printf("EVP_DecryptUpdate failed\n");
		return 0;
	}
	decryptedtext_len = len;

	if (1 != EVP_DecryptFinal_ex(ctx, decryptedtext + len, &len)) {
		printf("EVP_DecryptFinal_ex failed\n");
		return 0;
	}
	decryptedtext_len += len;

	decryptedtext[decryptedtext_len] = '\0';  // Null-terminate the decrypted text

	printf("Decrypted text: %s\n", decryptedtext);

	// Clean up
	EVP_CIPHER_CTX_free(ctx);

	return 0;
}


