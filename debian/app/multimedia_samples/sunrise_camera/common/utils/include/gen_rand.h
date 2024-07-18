#ifndef GEN_RAND_H
#define GEN_RAND_H

void init_rand(unsigned char *randevent, unsigned int randlen);
void init_rand_ex();
void gen_rand(unsigned char* buf, unsigned int buflen);

#endif