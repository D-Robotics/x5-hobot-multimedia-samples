#include "vdk_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>



#define VDKS_TRUE    1
#define VDKS_FALSE    0
int VDKS_Val_WindowsWidth    = 320;
int VDKS_Val_WindowsHeight    = 240;

int vdk_init(vdkEGL * vdk_egl)
{
    EGLint configAttribs[] =
    {
        EGL_RED_SIZE,       8,
        EGL_GREEN_SIZE,     8,
        EGL_BLUE_SIZE,      8,
        EGL_ALPHA_SIZE,     8,
        EGL_DEPTH_SIZE,     24,
        EGL_STENCIL_SIZE,   EGL_DONT_CARE,
        EGL_SAMPLE_BUFFERS, EGL_DONT_CARE,
        EGL_SAMPLES,        EGL_DONT_CARE,
        EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
        EGL_NONE
    };

    EGLint ctxAttribs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    memset(vdk_egl, 0, sizeof(vdkEGL));

    if(1 != vdkSetupEGL(100, 100, VDKS_Val_WindowsWidth, VDKS_Val_WindowsHeight, configAttribs, NULL, ctxAttribs, vdk_egl))
    {
        return VDKS_FALSE;
    }

    /* Adjust the window size to make sure these size values does not go beyound the screen limits. */
    vdkGetWindowInfo((*vdk_egl).window, NULL, NULL, &VDKS_Val_WindowsWidth, &VDKS_Val_WindowsHeight, NULL, NULL);

    return VDKS_TRUE;
}