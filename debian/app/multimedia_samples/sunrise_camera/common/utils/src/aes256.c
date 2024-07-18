#include "aes256.h"
#include "sha256.h"
#include "gen_rand.h"

#include <malloc.h>
#include <memory.h>
#include <stddef.h>

// This is an independent implementation of the encryption algorithm:   
//                                                                      
//         RIJNDAEL by Joan Daemen and Vincent Rijmen                   
//                                                                      
// which is a candidate algorithm in the Advanced Encryption Standard   
// programme of the US National Institute of Standards and Technology.  
//                                                                      
// Copyright in this implementation is held by Dr B R Gladman but I     
// hereby give permission for its free direct or derivative use subject 
// to acknowledgment of its origin and compliance with any conditions   
// that the originators of the algorithm place on its exploitation.     
//                                                                      
// Dr Brian Gladman (gladman@seven77.demon.co.uk) 14th January 1999     

//  Algorithm rijndael (rijndael.cpp)
//  128 bit key:
//  Key Setup:    223/1416 cycles (encrypt/decrypt)
//  Encrypt:       362 cycles =    70.7 mbits/sec
//  Decrypt:       367 cycles =    69.8 mbits/sec
//  Mean:          365 cycles =    70.2 mbits/sec
//  192 bit key:
//  Key Setup:    214/1660 cycles (encrypt/decrypt)
//  Encrypt:       442 cycles =    57.9 mbits/sec
//  Decrypt:       432 cycles =    59.3 mbits/sec
//  Mean:          437 cycles =    58.6 mbits/sec
//  256 bit key:
//  Key Setup:    287/1994 cycles (encrypt/decrypt)
//  Encrypt:       502 cycles =    51.0 mbits/sec
//  Decrypt:       506 cycles =    50.6 mbits/sec
//  Mean:          504 cycles =    50.8 mbits/sec


typedef unsigned char u1byte;
//typedef unsigned long u4byte;
typedef unsigned int u4byte;

typedef struct aes_context{
    u4byte k_len;
	u4byte e_key[64];
	u4byte d_key[64];
} aes_context; 

#define LARGE_TABLES


u1byte  pow_tab[256];
u1byte  log_tab[256];
u1byte  sbx_tab[256];
u1byte  isb_tab[256];
u4byte  rco_tab[ 10];
u4byte  ft_tab[4][256];
u4byte  it_tab[4][256];

#ifdef  LARGE_TABLES
  u4byte  fl_tab[4][256];
  u4byte  il_tab[4][256];
#endif

u4byte  tab_gen = 0;

#define byte(x,n)   ((u1byte)((x) >> (8 * n))) 
#define rotl(x,n) (((x) << ((int)(n))) | ((x) >> (32 -(int)(n))))
#define rotr(x,n) (((x) >> ((int)(n))) | ((x) << (32 -(int)(n))))
//#define bswap(x)    (rotl(x, 8) & 0x00ff00ff | rotr(x, 8) & 0xff00ff00)
#define u4byte_in(x) (*(u4byte*)(x)) 
#define u4byte_out(x, v) (*(u4byte*)(x) = (v)) 

#define ff_mult(a,b)    (a && b ? pow_tab[(log_tab[a] + log_tab[b]) % 255] : 0)

#define f_rn(bo, bi, n, k)                          \
    bo[n] =  ft_tab[0][byte(bi[n],0)] ^             \
             ft_tab[1][byte(bi[(n + 1) & 3],1)] ^   \
             ft_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
             ft_tab[3][byte(bi[(n + 3) & 3],3)] ^ *(k + n)

#define i_rn(bo, bi, n, k)                          \
    bo[n] =  it_tab[0][byte(bi[n],0)] ^             \
             it_tab[1][byte(bi[(n + 3) & 3],1)] ^   \
             it_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
             it_tab[3][byte(bi[(n + 1) & 3],3)] ^ *(k + n)

#ifdef LARGE_TABLES

#define ls_box(x)                \
    ( fl_tab[0][byte(x, 0)] ^    \
      fl_tab[1][byte(x, 1)] ^    \
      fl_tab[2][byte(x, 2)] ^    \
      fl_tab[3][byte(x, 3)] )

#define f_rl(bo, bi, n, k)                          \
    bo[n] =  fl_tab[0][byte(bi[n],0)] ^             \
             fl_tab[1][byte(bi[(n + 1) & 3],1)] ^   \
             fl_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
             fl_tab[3][byte(bi[(n + 3) & 3],3)] ^ *(k + n)

#define i_rl(bo, bi, n, k)                          \
    bo[n] =  il_tab[0][byte(bi[n],0)] ^             \
             il_tab[1][byte(bi[(n + 3) & 3],1)] ^   \
             il_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
             il_tab[3][byte(bi[(n + 1) & 3],3)] ^ *(k + n)

#else

#define ls_box(x)                            \
    ((u4byte)sbx_tab[byte(x, 0)] <<  0) ^    \
    ((u4byte)sbx_tab[byte(x, 1)] <<  8) ^    \
    ((u4byte)sbx_tab[byte(x, 2)] << 16) ^    \
    ((u4byte)sbx_tab[byte(x, 3)] << 24)

#define f_rl(bo, bi, n, k)                                      \
    bo[n] = (u4byte)sbx_tab[byte(bi[n],0)] ^                    \
        rotl(((u4byte)sbx_tab[byte(bi[(n + 1) & 3],1)]),  8) ^  \
        rotl(((u4byte)sbx_tab[byte(bi[(n + 2) & 3],2)]), 16) ^  \
        rotl(((u4byte)sbx_tab[byte(bi[(n + 3) & 3],3)]), 24) ^ *(k + n)

#define i_rl(bo, bi, n, k)                                      \
    bo[n] = (u4byte)isb_tab[byte(bi[n],0)] ^                    \
        rotl(((u4byte)isb_tab[byte(bi[(n + 3) & 3],1)]),  8) ^  \
        rotl(((u4byte)isb_tab[byte(bi[(n + 2) & 3],2)]), 16) ^  \
        rotl(((u4byte)isb_tab[byte(bi[(n + 1) & 3],3)]), 24) ^ *(k + n)

#endif

void gen_tabs(void)
{   
	u4byte  i, t;
    u1byte  p, q;

    // log and power tables for GF(2**8) finite field with  
    // 0x011b as modular polynomial - the simplest prmitive 
    // root is 0x03, used here to generate the tables       

    for(i = 0,p = 1; i < 256; ++i)
    {
        pow_tab[i] = (u1byte)p; log_tab[p] = (u1byte)i;

        p = p ^ (p << 1) ^ (p & 0x80 ? 0x01b : 0);
    }

    log_tab[1] = 0; p = 1;

    for(i = 0; i < 10; ++i)
    {
        rco_tab[i] = p; 

        p = (p << 1) ^ (p & 0x80 ? 0x1b : 0);
    }

    for(i = 0; i < 256; ++i)
    {   
        p = (i ? pow_tab[255 - log_tab[i]] : 0); q = p; 
        q = (q >> 7) | (q << 1); p ^= q; 
        q = (q >> 7) | (q << 1); p ^= q; 
        q = (q >> 7) | (q << 1); p ^= q; 
        q = (q >> 7) | (q << 1); p ^= q ^ 0x63; 
        sbx_tab[i] = p; isb_tab[p] = (u1byte)i;
    }

    for(i = 0; i < 256; ++i)
    {
        p = sbx_tab[i]; 

#ifdef  LARGE_TABLES        
        
        t = p; fl_tab[0][i] = t;
        fl_tab[1][i] = rotl(t,  8);
        fl_tab[2][i] = rotl(t, 16);
        fl_tab[3][i] = rotl(t, 24);
#endif
        t = ((u4byte)ff_mult(2, p)) |
            ((u4byte)p <<  8) |
            ((u4byte)p << 16) |
            ((u4byte)ff_mult(3, p) << 24);
        
        ft_tab[0][i] = t;
        ft_tab[1][i] = rotl(t,  8);
        ft_tab[2][i] = rotl(t, 16);
        ft_tab[3][i] = rotl(t, 24);

        p = isb_tab[i]; 

#ifdef  LARGE_TABLES        
        
        t = p; il_tab[0][i] = t; 
        il_tab[1][i] = rotl(t,  8); 
        il_tab[2][i] = rotl(t, 16); 
        il_tab[3][i] = rotl(t, 24);
#endif 
        t = ((u4byte)ff_mult(14, p)) |
            ((u4byte)ff_mult( 9, p) <<  8) |
            ((u4byte)ff_mult(13, p) << 16) |
            ((u4byte)ff_mult(11, p) << 24);
        
        it_tab[0][i] = t; 
        it_tab[1][i] = rotl(t,  8); 
        it_tab[2][i] = rotl(t, 16); 
        it_tab[3][i] = rotl(t, 24); 
    }

    tab_gen = 1;
}

#define star_x(x) (((x) & 0x7f7f7f7f) << 1) ^ ((((x) & 0x80808080) >> 7) * 0x1b)

#define imix_col(y,x)       \
    u   = star_x(x);        \
    v   = star_x(u);        \
    w   = star_x(v);        \
    t   = w ^ (x);          \
   (y)  = u ^ v ^ w;        \
   (y) ^= rotr(u ^ t,  8) ^ \
          rotr(v ^ t, 16) ^ \
          rotr(t,24)



// initialise the key schedule from the user supplied key   

#define loop4(i)                                    \
{   t = ls_box(rotr(t,  8)) ^ rco_tab[i];           \
    t ^= ctx->e_key[4 * i];     ctx->e_key[4 * i + 4] = t;    \
    t ^= ctx->e_key[4 * i + 1]; ctx->e_key[4 * i + 5] = t;    \
    t ^= ctx->e_key[4 * i + 2]; ctx->e_key[4 * i + 6] = t;    \
    t ^= ctx->e_key[4 * i + 3]; ctx->e_key[4 * i + 7] = t;    \
}

#define loop6(i)                                    \
{   t = ls_box(rotr(t,  8)) ^ rco_tab[i];           \
    t ^= ctx->e_key[6 * i];     ctx->e_key[6 * i + 6] = t;    \
    t ^= ctx->e_key[6 * i + 1]; ctx->e_key[6 * i + 7] = t;    \
    t ^= ctx->e_key[6 * i + 2]; ctx->e_key[6 * i + 8] = t;    \
    t ^= ctx->e_key[6 * i + 3]; ctx->e_key[6 * i + 9] = t;    \
    t ^= ctx->e_key[6 * i + 4]; ctx->e_key[6 * i + 10] = t;   \
    t ^= ctx->e_key[6 * i + 5]; ctx->e_key[6 * i + 11] = t;   \
}

#define loop8(i)                                    \
{   t = ls_box(rotr(t,  8)) ^ rco_tab[i];           \
    t ^= ctx->e_key[8 * i];     ctx->e_key[8 * i + 8] = t;    \
    t ^= ctx->e_key[8 * i + 1]; ctx->e_key[8 * i + 9] = t;    \
    t ^= ctx->e_key[8 * i + 2]; ctx->e_key[8 * i + 10] = t;   \
    t ^= ctx->e_key[8 * i + 3]; ctx->e_key[8 * i + 11] = t;   \
    t  = ctx->e_key[8 * i + 4] ^ ls_box(t);              \
    ctx->e_key[8 * i + 12] = t;                          \
    t ^= ctx->e_key[8 * i + 5]; ctx->e_key[8 * i + 13] = t;   \
    t ^= ctx->e_key[8 * i + 6]; ctx->e_key[8 * i + 14] = t;   \
    t ^= ctx->e_key[8 * i + 7]; ctx->e_key[8 * i + 15] = t;   \
}

void aes_init(const u1byte in_key[], const u4byte key_len, aes_context *ctx)
{   
	u4byte  i, t, u, v, w;

    if(!tab_gen)
	{
        gen_tabs();
	}

    ctx->k_len = (key_len + 31) / 32;

    ctx->e_key[0] = u4byte_in(in_key     ); 
	ctx->e_key[1] = u4byte_in(in_key +  4);
    ctx->e_key[2] = u4byte_in(in_key +  8); 
	ctx->e_key[3] = u4byte_in(in_key + 12);

    switch(ctx->k_len)
    {
        case 4: t = ctx->e_key[3];
                for(i = 0; i < 10; ++i) 
                    loop4(i);
                break;

        case 6: ctx->e_key[4] = u4byte_in(in_key + 16); t = ctx->e_key[5] = u4byte_in(in_key + 20);
                for(i = 0; i < 8; ++i) 
                    loop6(i);
                break;

        case 8: ctx->e_key[4] = u4byte_in(in_key + 16); ctx->e_key[5] = u4byte_in(in_key + 20);
                ctx->e_key[6] = u4byte_in(in_key + 24); t = ctx->e_key[7] = u4byte_in(in_key + 28);
                for(i = 0; i < 7; ++i) 
                    loop8(i);
                break;
    }

    ctx->d_key[0] = ctx->e_key[0]; ctx->d_key[1] = ctx->e_key[1];
    ctx->d_key[2] = ctx->e_key[2]; ctx->d_key[3] = ctx->e_key[3];

    for(i = 4; i < 4 * ctx->k_len + 24; ++i)
    {
        imix_col(ctx->d_key[i], ctx->e_key[i]);
    }

    return;
}

// encrypt a block of text  

#define f_nround(bo, bi, k) \
    f_rn(bo, bi, 0, k);     \
    f_rn(bo, bi, 1, k);     \
    f_rn(bo, bi, 2, k);     \
    f_rn(bo, bi, 3, k);     \
    k += 4

#define f_lround(bo, bi, k) \
    f_rl(bo, bi, 0, k);     \
    f_rl(bo, bi, 1, k);     \
    f_rl(bo, bi, 2, k);     \
    f_rl(bo, bi, 3, k)

void aes_encrypt_ecb(const u1byte in_blk[16], u1byte out_blk[16], aes_context *ctx)
{   
	u4byte  b0[4], b1[4], *kp;

    b0[0] = u4byte_in(in_blk    ) ^ ctx->e_key[0]; b0[1] = u4byte_in(in_blk +  4) ^ ctx->e_key[1];
    b0[2] = u4byte_in(in_blk + 8) ^ ctx->e_key[2]; b0[3] = u4byte_in(in_blk + 12) ^ ctx->e_key[3];

    kp = ctx->e_key + 4;

    if(ctx->k_len > 6)
    {
        f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    }

    if(ctx->k_len > 4)
    {
        f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    }

    f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    f_nround(b1, b0, kp); f_lround(b0, b1, kp);

    u4byte_out(out_blk,      b0[0]); u4byte_out(out_blk +  4, b0[1]);
    u4byte_out(out_blk +  8, b0[2]); u4byte_out(out_blk + 12, b0[3]);
}

// decrypt a block of text  

#define i_nround(bo, bi, k) \
    i_rn(bo, bi, 0, k);     \
    i_rn(bo, bi, 1, k);     \
    i_rn(bo, bi, 2, k);     \
    i_rn(bo, bi, 3, k);     \
    k -= 4

#define i_lround(bo, bi, k) \
    i_rl(bo, bi, 0, k);     \
    i_rl(bo, bi, 1, k);     \
    i_rl(bo, bi, 2, k);     \
    i_rl(bo, bi, 3, k)

void aes_decrypt_ecb(const u1byte in_blk[16], u1byte out_blk[16], aes_context *ctx)
{   
	u4byte  b0[4], b1[4], *kp;

    b0[0] = u4byte_in(in_blk     ) ^ ctx->e_key[4 * ctx->k_len + 24]; 
	b0[1] = u4byte_in(in_blk +  4) ^ ctx->e_key[4 * ctx->k_len + 25];
    b0[2] = u4byte_in(in_blk +  8) ^ ctx->e_key[4 * ctx->k_len + 26]; 
	b0[3] = u4byte_in(in_blk + 12) ^ ctx->e_key[4 * ctx->k_len + 27];

    kp = ctx->d_key + 4 * (ctx->k_len + 5);

    if(ctx->k_len > 6)
    {
        i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    }

    if(ctx->k_len > 4)
    {
        i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    }

    i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    i_nround(b1, b0, kp); i_lround(b0, b1, kp);

    u4byte_out(out_blk,     b0[0]); u4byte_out(out_blk +  4, b0[1]);
    u4byte_out(out_blk + 8, b0[2]); u4byte_out(out_blk + 12, b0[3]);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const int BLOCK_SIZE	= 16;
#define BLOCK_SIZE_EX	16


long aes256_cbc_enc_by_head(unsigned char *head, unsigned int headLen, unsigned char *data, unsigned int dataLen, 
							unsigned char **encData, unsigned int *encDataLen, unsigned char key[32])
{
	int iBlocks;
	unsigned char IV[32];
	aes_context ctx;
	unsigned char *pEncData;
	unsigned char *pData;
	int i, j;

	if(encData == NULL || encDataLen == NULL)
	{
		return -1;
	}

	iBlocks = (dataLen + BLOCK_SIZE*2)/BLOCK_SIZE;
    *encDataLen = iBlocks * BLOCK_SIZE;
	*encData = malloc(*encDataLen);
	if(*encData == NULL)
	{
		return -2;
	}
	
	sha256_mac(head, headLen, IV);

	memset(*encData,0,*encDataLen);
	memcpy(*encData,IV,BLOCK_SIZE);
	memcpy(*encData + BLOCK_SIZE,data,dataLen);	
	(*encData)[*encDataLen - 1] = *encDataLen - dataLen - BLOCK_SIZE;

	aes_init(key,256,&ctx);

	pEncData = *encData;
	pData = pEncData + BLOCK_SIZE;
	for(i = 0; i < iBlocks; i++)
	{
		aes_encrypt_ecb(pEncData,pEncData,&ctx);

		if(i != iBlocks - 1)
		{
			for(j = 0; j < BLOCK_SIZE/sizeof(u4byte); j++)
			{
				*((u4byte*)pData) ^= *((u4byte*)pEncData);
				pEncData += sizeof(u4byte);
				pData += sizeof(u4byte);
			}	
		}		
	}

	memset(&ctx,0,sizeof(ctx));

	return 0;
}

long aes256_cbc_enc(unsigned char *data, unsigned int dataLen, unsigned char **encData, unsigned int *encDataLen, unsigned char key[32])
{
	int iBlocks;
	unsigned char IV[BLOCK_SIZE_EX];
	aes_context ctx;
	unsigned char *pEncData;
	unsigned char *pData;
	int i, j;

	if(encData == NULL || encDataLen == NULL)
	{
		return -1;
	}

	iBlocks = (dataLen + BLOCK_SIZE*2)/BLOCK_SIZE;
    *encDataLen = iBlocks * BLOCK_SIZE;
	*encData = malloc(*encDataLen);
	if(*encData == NULL)
	{
		return -2;
	}
	
	gen_rand(IV, BLOCK_SIZE);

	memset(*encData, 0, *encDataLen);
	memcpy(*encData, IV, BLOCK_SIZE);
	memcpy(*encData + BLOCK_SIZE, data, dataLen);	
	(*encData)[*encDataLen - 1] = *encDataLen - dataLen - BLOCK_SIZE;

	aes_init(key, 256, &ctx);

	pEncData = *encData;
	pData = pEncData + BLOCK_SIZE;
	for(i = 0; i < iBlocks; i++)
	{
		aes_encrypt_ecb(pEncData, pEncData, &ctx);

		if(i != iBlocks - 1)
		{
			for(j = 0; j < BLOCK_SIZE/sizeof(u4byte); j++)
			{
				*((u4byte*)pData) ^= *((u4byte*)pEncData);
				pEncData += sizeof(u4byte);
				pData += sizeof(u4byte);
			}	
		}		
	}

	memset(&ctx,0,sizeof(ctx));

	return 0;
}

long aes256_cbc_dec(unsigned char *data, unsigned int dataLen, unsigned char **decData, unsigned int *decDataLen, unsigned char key[32])
{
	int iBlocks;
	aes_context ctx;
	unsigned char *pDecData;
	unsigned char *pData;
	int i, j;

	if(decData == NULL || decDataLen == NULL)
	{
		return -1;
	}
	if(dataLen%BLOCK_SIZE != 0)
	{
		return -2;
	}

	iBlocks = dataLen/BLOCK_SIZE;
	if(iBlocks < 2)
	{
		return -3;
	}

	*decData = malloc(dataLen - BLOCK_SIZE + 1);
	if(*decData == NULL)
	{
		return -2;
	}
	memcpy(*decData, data + BLOCK_SIZE, dataLen - BLOCK_SIZE);	

	aes_init(key, 256, &ctx);

	pDecData = *decData;
	pData = data;

	for(i = 1;i < iBlocks;i++)
	{
		aes_decrypt_ecb(pDecData,pDecData,&ctx);

		for(j = 0;j < BLOCK_SIZE/sizeof(u4byte);j++)
		{
			*((u4byte*)pDecData) ^= *((u4byte*)pData);
			pDecData += sizeof(u4byte);
			pData += sizeof(u4byte);			
		}		
	}

	if(*(pDecData-1) > BLOCK_SIZE)
	{
		free(*decData);
		*decData = NULL;
		return -4;
	}

	*decDataLen = dataLen - BLOCK_SIZE - *(pDecData-1);
	(*decData)[*decDataLen] = 0;
	(*decData)[(*decDataLen) + 1] = 0;

	memset(&ctx,0,sizeof(ctx));

	return 0;
}

