#ifndef GPU_3D_COMMON_BMP_HH_
#define GPU_3D_COMMON_BMP_HH_
typedef struct RGB {                      /**** Colormap entry structure ****/
    unsigned char   rgbBlue;          /* Blue value */
    unsigned char   rgbGreen;         /* Green value */
    unsigned char   rgbRed;           /* Red value */
    unsigned char   rgbReserved;      /* Reserved */
}
RGB;

typedef struct BMPINFOHEADER {                     /**** BMP file info structure ****/
    unsigned int   biSize;           /* Size of info header */
    int            biWidth;          /* Width of image */
    int            biHeight;         /* Height of image */
    unsigned short biPlanes;         /* Number of color planes */
    unsigned short biBitCount;       /* Number of bits per pixel */
    unsigned int   biCompression;    /* Type of compression to use */
    unsigned int   biSizeImage;      /* Size of image data */
    int            biXPelsPerMeter;  /* X pixels per meter */
    int            biYPelsPerMeter;  /* Y pixels per meter */
    unsigned int   biClrUsed;        /* Number of colors used */
    unsigned int   biClrImportant;   /* Number of important colors */
}
BMPINFOHEADER;

typedef struct BMPINFO {                      /**** Bitmap information structure ****/
    BMPINFOHEADER   bmiHeader;      /* Image header */
    RGB             bmiColors[256]; /* Image colormap */
}
BMPINFO;

void SaveBmpImage( const char* fileName, int width, int height, void* pixels );
#endif // !GPU_3D_COMMON_BMP_HH_