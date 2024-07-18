#ifndef __ENCRYPT_DEF__
#define __ENCRYPT_DEF__

#ifdef __cplusplus
extern "C" {
#endif

long aes256_cbc_enc_by_head(unsigned char *head, unsigned int headLen, unsigned char *data, unsigned int dataLen,
		unsigned char **encData, unsigned int *encDataLen, unsigned char key[32]);
long aes256_cbc_enc(unsigned char *data, unsigned int dataLen, unsigned char **encData, unsigned int *encDataLen, unsigned char key[32]);
long aes256_cbc_dec(unsigned char *data, unsigned int dataLen, unsigned char **decData, unsigned int *decDataLen, unsigned char key[32]);


#ifdef __cplusplus
}
#endif

#endif
