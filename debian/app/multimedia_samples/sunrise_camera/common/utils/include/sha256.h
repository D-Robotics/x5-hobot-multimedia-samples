#ifndef SHA256_H
#define SHA256_H

#ifndef uint8
#define uint8  unsigned char
#endif

#ifndef uint32
#define uint32 unsigned long int
#endif

typedef struct
{
    uint32 total[2];
    uint32 state[8];
    uint8 buffer[64];
}sha256_context;

void sha256_starts(sha256_context *ctx );
void sha256_process(sha256_context *ctx, uint8 data[64]);
void sha256_update(sha256_context *ctx, uint8 *input, uint32 length );
void sha256_finish(sha256_context *ctx, uint8 digest[32]);
void sha256_mac(unsigned char *data, unsigned int dataLen, unsigned char mac[32]);

#endif