#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bmp.h"

/*
 * BMP Operations.
*/

#define BF_TYPE 0x4D42             /* "MB" */



typedef struct BMPFILEHEADER {                      /**** BMP file header structure ****/
    unsigned short bfType;           /* Magic number for file */
    unsigned int   bfSize;           /* Size of file */
    unsigned short bfReserved1;      /* Reserved */
    unsigned short bfReserved2;      /* ... */
    unsigned int   bfOffBits;        /* Offset to bitmap data */
}
BMPFILEHEADER;

#define BIT_RGB       0             /* No compression - straight BGR data */
#define BIT_RLE8      1             /* 8-bit run-length compression */
#define BIT_RLE4      2             /* 4-bit run-length compression */
#define BIT_BITFIELDS 3             /* RGB bitmap with RGB masks */

/*
 * 'write_word()' - Write a 16-bit unsigned integer.
 */
static int write_word(FILE *fp, unsigned short w)
{
    putc(w, fp);
    return (putc(w >> 8, fp));
}

/*
 * 'write_dword()' - Write a 32-bit unsigned integer.
 */
static int write_dword(FILE *fp, unsigned int dw)
{
    putc(dw, fp);
    putc(dw >> 8, fp);
    putc(dw >> 16, fp);
    return (putc(dw >> 24, fp));
}

/*
 * 'write_long()' - Write a 32-bit signed integer.
 */
static int write_long(FILE *fp,int  l)
{
    putc(l, fp);
    putc(l >> 8, fp);
    putc(l >> 16, fp);
    return (putc(l >> 24, fp));
}

/*
 * 'SaveDIBitmap()' - Save a DIB/BMP file to disk.
 *
 * Returns 0 on success or -1 on failure...
 */

int GltSaveDIBitmap(const char *filename, BMPINFO *info, unsigned char *bits)
{
    FILE *fp;                      /* Open file pointer */
    unsigned int    size,                     /* Size of file */
                    infosize,                 /* Size of bitmap info */
                    bitsize;                  /* Size of bitmap pixels */


    /* Try opening the file; use "wb" mode to write this *binary* file. */
    if ((fp = fopen(filename, "wb")) == NULL)
        return (-1);

    /* Figure out the bitmap size */
    if (info->bmiHeader.biSizeImage == 0)
        bitsize =  (info->bmiHeader.biWidth *
                    info->bmiHeader.biBitCount + 7) / 8 *
                abs(info->bmiHeader.biHeight);
    else
        bitsize = info->bmiHeader.biSizeImage;

    /* Figure out the header size */
    infosize = sizeof(BMPINFOHEADER);
    switch (info->bmiHeader.biCompression)
    {
    case BIT_BITFIELDS :
        infosize += 12; /* Add 3 RGB doubleword masks */
        if (info->bmiHeader.biClrUsed == 0)
        break;
    case BIT_RGB :
        if (info->bmiHeader.biBitCount > 8 &&
        info->bmiHeader.biClrUsed == 0)
        break;
    case BIT_RLE8 :
    case BIT_RLE4 :
        if (info->bmiHeader.biClrUsed == 0)
            infosize += (1 << info->bmiHeader.biBitCount) * 4;
        else
            infosize += info->bmiHeader.biClrUsed * 4;
        break;
    }

    size = sizeof(BMPFILEHEADER) + infosize + bitsize;

    /* Write the file header, bitmap information, and bitmap pixel data... */
    write_word(fp, BF_TYPE);        /* bfType */
    write_dword(fp, size);          /* bfSize */
    write_word(fp, 0);              /* bfReserved1 */
    write_word(fp, 0);              /* bfReserved2 */
    write_dword(fp, 14 + infosize); /* bfOffBits */

    write_dword(fp, info->bmiHeader.biSize);
    write_long(fp, info->bmiHeader.biWidth);
    write_long(fp, info->bmiHeader.biHeight);
    write_word(fp, info->bmiHeader.biPlanes);
    write_word(fp, info->bmiHeader.biBitCount);
    write_dword(fp, info->bmiHeader.biCompression);
    write_dword(fp, info->bmiHeader.biSizeImage);
    write_long(fp, info->bmiHeader.biXPelsPerMeter);
    write_long(fp, info->bmiHeader.biYPelsPerMeter);
    write_dword(fp, info->bmiHeader.biClrUsed);
    write_dword(fp, info->bmiHeader.biClrImportant);

    if (infosize > 40)
    if (fwrite(info->bmiColors, infosize - 40, 1, fp) < 1)
    {
        /* Couldn't write the bitmap header - return... */
        fclose(fp);
        return (-1);
    }

    if (fwrite(bits, 1, bitsize, fp) < bitsize)
    {
        /* Couldn't write the bitmap - return... */
        fclose(fp);
        return (-1);
    }

    /* OK, everything went fine - return... */
    fclose(fp);

    return (0);
}


void SaveBmpImage( const char* fileName, int width, int height, void* pixels )
{
    unsigned char* pBase = (unsigned char*)pixels;
    unsigned char temp;
    int pixels_count = width *height;

    int i;

    BMPINFO  bmpInfo;
    bmpInfo.bmiHeader.biSize = sizeof( bmpInfo.bmiHeader );
    bmpInfo.bmiHeader.biBitCount = 32;        //RGBA8888
    bmpInfo.bmiHeader.biWidth = width;
    bmpInfo.bmiHeader.biHeight = height;
    bmpInfo.bmiHeader.biSizeImage = width * height * 4;
    bmpInfo.bmiHeader.biCompression = BIT_RGB;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biClrUsed = 0;
    bmpInfo.bmiHeader.biClrImportant = 0;
    bmpInfo.bmiHeader.biXPelsPerMeter = bmpInfo.bmiHeader.biYPelsPerMeter = 0;


    for(i = 0; i < pixels_count; ++i ){        //Swap Blue and Red.
        temp = pBase[0];
        pBase[0] = pBase[2];
        pBase[2] = temp;
        pBase += 4;
    }
    GltSaveDIBitmap( fileName, &bmpInfo, (unsigned char*)pixels );
}
