#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

static void print_help()
{
	printf("Usage: sample_rsa [Options] [value]\n");
	printf("Options:\n");
	printf("  -k <key size>     Key Size [1024/2048/3072/4096/8192]\n");
	printf("  -h                Show this help message\n");
}

void handleErrors() {
	fprintf(stderr, "Error occurred\n");
	ERR_print_errors_fp(stderr);
	exit(1);
}

void print_private_key(RSA *rsa) {
	// 打印私钥
	BIO *bio = BIO_new_fp(stdout, BIO_NOCLOSE);  // 用于将数据打印到标准输出
	if (bio == NULL) {
		fprintf(stderr, "BIO_new_fp failed\n");
		return;
	}
	PEM_write_bio_RSAPrivateKey(bio, rsa, NULL, NULL, 0, NULL, NULL);
	BIO_free(bio);
}

void print_public_key(RSA *rsa) {
	// 打印公钥
	BIO *bio = BIO_new_fp(stdout, BIO_NOCLOSE);  // 用于将数据打印到标准输出
	if (bio == NULL) {
		fprintf(stderr, "BIO_new_fp failed\n");
		return;
	}
	PEM_write_bio_RSA_PUBKEY(bio, rsa);
	BIO_free(bio);
}

int main(int argc, char **argv)
{
	int key_size = 0;
	if (argc != 3) {
		print_help();
		return 0;
	}

	if (0 == strcmp(argv[1], "-k")) {
		key_size = atoi(argv[2]);
		if (key_size != 1024 && key_size != 2048 && key_size != 3072 && key_size != 4096 && key_size != 8192) {
			printf("Not Support Key Size: %d\n", key_size);
			print_help();
			return 0;
		}
	}
	else {
		print_help();
		return 0;
	}

	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();

	// 1. 生成 RSA 密钥对
	RSA *rsa = RSA_new();
	BIGNUM *bn = BN_new();
	if (bn == NULL) handleErrors();

	if (BN_set_word(bn, RSA_F4) != 1) handleErrors();  // RSA_F4 是常用的公钥指数 65537
	if (RSA_generate_key_ex(rsa, key_size, bn, NULL) != 1) handleErrors();

	// 2. 提取公钥和私钥
	BIO *pub = BIO_new(BIO_s_mem());
	BIO *priv = BIO_new(BIO_s_mem());

	if (PEM_write_bio_RSAPublicKey(pub, rsa) != 1) handleErrors();
	if (PEM_write_bio_RSAPrivateKey(priv, rsa, NULL, NULL, 0, NULL, NULL) != 1) handleErrors();

	// 打印公钥私钥
	print_private_key(rsa);
	print_public_key(rsa);

	// 3. 加密数据
	unsigned char plaintext[128] = {0};
	unsigned char ciphertext[RSA_size(rsa)];
	int ciphertext_len;
	snprintf((char *)plaintext, 128, "The test message for RSA-%d encryption!", key_size);

	EVP_PKEY *pubkey = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(pubkey, rsa);  // 将 RSA 密钥分配给 EVP_PKEY 对象

	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pubkey, NULL);
	if (EVP_PKEY_encrypt_init(ctx) <= 0) handleErrors();

	size_t outlen;
	if (EVP_PKEY_encrypt(ctx, ciphertext, &outlen, plaintext, strlen((char *)plaintext)) <= 0) handleErrors();
	ciphertext_len = outlen;

	printf("\nEncrypted text:\n");
	for (int i = 0; i < ciphertext_len; i++) {
		printf("%02x", ciphertext[i]);
	}
	printf("\n\n");

	// 4. 解密数据
	unsigned char decryptedtext[2048];
	EVP_PKEY *privkey = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(privkey, rsa);  // 将私钥分配给 EVP_PKEY 对象

	EVP_PKEY_CTX *dec_ctx = EVP_PKEY_CTX_new(privkey, NULL);
	if (EVP_PKEY_decrypt_init(dec_ctx) <= 0) handleErrors();

	size_t decrypted_len;
	if (EVP_PKEY_decrypt(dec_ctx, decryptedtext, &decrypted_len, ciphertext, ciphertext_len) <= 0) handleErrors();

	decryptedtext[decrypted_len] = '\0';  // 确保解密后的数据是以 \0 结尾的

	printf("Decrypted text: %s\n", decryptedtext);

	// 清理
	EVP_PKEY_free(pubkey);
	EVP_PKEY_free(privkey);
	BIO_free_all(pub);
	BIO_free_all(priv);
	BN_free(bn);

	// 释放 RSA 对象
	RSA_free(rsa);

	// 清理 OpenSSL
	EVP_cleanup();
	ERR_free_strings();

	return 0;
}

