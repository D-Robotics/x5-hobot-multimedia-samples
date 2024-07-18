#ifndef __BASE64__
#define __BASE64__

#ifdef __cplusplus
extern "C" {
#endif

int base64_encode(unsigned char *data, unsigned int dataLen, unsigned char **encData, unsigned int *encDataLen);
int base64_decode(unsigned char *data, unsigned int dataLen, unsigned char **decData, unsigned int *decDataLen);

#ifdef __cplusplus
}
#endif

#endif
