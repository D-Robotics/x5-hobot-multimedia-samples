#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "base64.h"

static void decodeQuantum(unsigned char *dest, unsigned char *src)
{
  unsigned int x = 0;
  int i;
  for(i = 0; i < 4; i++) {
    if(src[i] >= 'A' && src[i] <= 'Z')
      x = (x << 6) + (unsigned int)(src[i] - 'A' + 0);
    else if(src[i] >= 'a' && src[i] <= 'z')
      x = (x << 6) + (unsigned int)(src[i] - 'a' + 26);
    else if(src[i] >= '0' && src[i] <= '9')
      x = (x << 6) + (unsigned int)(src[i] - '0' + 52);
    else if(src[i] == '+')
      x = (x << 6) + 62;
    else if(src[i] == '/')
      x = (x << 6) + 63;
    else if(src[i] == '=')
      x = (x << 6);
  }

  dest[2] = (unsigned char)(x & 255);
  x >>= 8;
  dest[1] = (unsigned char)(x & 255);
  x >>= 8;
  dest[0] = (unsigned char)(x & 255);
}

int base64_decode(unsigned char *data, unsigned int dataLen, unsigned char **decData, unsigned int *decDataLen)
{
	int equalsTerm = 0;
	int i;
	int numQuantums;
	unsigned char lastQuantum[3];
	unsigned char *newstr;

	if(dataLen % 4 != 0)
	{
		return -1;
	}

	/* Don't allocate a buffer if the decoded length is 0 */
	numQuantums = dataLen / 4;
	if (numQuantums <= 0)
		return -2;

	/* A maximum of two = padding characters is allowed */
	if(data[dataLen - 1] == '=') 
	{
		equalsTerm++;
		if(data[dataLen - 2] == '=')
			equalsTerm++;
	}  

	*decDataLen = (numQuantums * 3) - equalsTerm;

	/* The buffer must be large enough to make room for the last quantum
	(which may be partially thrown out) and the zero terminator. */
	*decData = newstr = (unsigned char *)malloc(*decDataLen+4);
	if(newstr == NULL)
		return -3;

	/* Decode all but the last quantum (which may not decode to a
	multiple of 3 bytes) */
	for(i = 0; i < numQuantums - 1; i++) {
		decodeQuantum((unsigned char *)newstr, data);
		newstr += 3; data += 4;
	}

	/* This final decode may actually read slightly past the end of the buffer
	if the input string is missing pad bytes.  This will almost always be
	harmless. */
	decodeQuantum(lastQuantum, data);
	for(i = 0; i < 3 - equalsTerm; i++)
		newstr[i] = lastQuantum[i];

	newstr[i] = 0;

	return 0;
}

static const char table64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#ifndef WIN32
#define _snprintf snprintf
#endif

int base64_encode(unsigned char *data, unsigned int dataLen, unsigned char **encData, unsigned int *encDataLen)
{
	unsigned char ibuf[3];
	unsigned char obuf[4];
	int i;
	int inputparts;
	unsigned char *output;

//	unsigned char *indata = data;

	*encData = output = (unsigned char *)malloc(dataLen*4/3+4);
	if(NULL == output)
		return -1;

	while(dataLen > 0)
	{
		for (i = inputparts = 0; i < 3; i++) 
		{
			if(dataLen > 0) 
			{
				inputparts++;
				ibuf[i] = *data;
				data++;
				dataLen--;
			}
			else
				ibuf[i] = 0;
		}

		obuf [0] = (ibuf [0] & 0xFC) >> 2;
		obuf [1] = ((ibuf [0] & 0x03) << 4) | ((ibuf [1] & 0xF0) >> 4);
		obuf [2] = ((ibuf [1] & 0x0F) << 2) | ((ibuf [2] & 0xC0) >> 6);
		obuf [3] = ibuf [2] & 0x3F;

		switch(inputparts) {
	case 1: /* only one byte read */
		_snprintf((char*)output, 5, "%c%c==",
			table64[obuf[0]],
			table64[obuf[1]]);
		break;
	case 2: /* two bytes read */
		_snprintf((char*)output, 5, "%c%c%c=",
			table64[obuf[0]],
			table64[obuf[1]],
			table64[obuf[2]]);
		break;
	default:
		_snprintf((char*)output, 5, "%c%c%c%c",
			table64[obuf[0]],
			table64[obuf[1]],
			table64[obuf[2]],
			table64[obuf[3]] );
		break;
		}
		output += 4;
	}
	*output=0;
	*encDataLen = strlen((char*)*encData);

	return 0; /* return the length of the new data */
}


