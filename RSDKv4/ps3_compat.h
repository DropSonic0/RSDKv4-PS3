#ifndef PS3_COMPAT_H
#define PS3_COMPAT_H

#if defined(PS3) || defined(__PS3__) || defined(__CELLOS_LV2__)

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>

#include <sys/sys_time.h>
#include <sys/process.h>
#include <sys/timer.h>
#include <sys/return_code.h>

#include <cell/sysmodule.h>
#include <cell/pad.h>
#include <cell/audio.h>
#include <cell/dbgfont.h>
#include <cell/resc.h>
#include <cell/gcm.h>

#ifndef BYTE_DEFINED
#define BYTE_DEFINED
typedef unsigned char byte;
#endif

typedef signed short Sint16;
typedef signed int Sint32;
typedef signed long long Sint64;
typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef unsigned int Uint32;
typedef unsigned long long Uint64;

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__PS3__) || defined(PS3) || defined(__CELLOS_LV2__)
#undef strcasecmp
#undef strncasecmp
#endif

#ifndef _SNPRINTF_DEFINED
#define _SNPRINTF_DEFINED
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif

#define strcasecmp strcasecmp_ps3
#define strncasecmp strncasecmp_ps3

static inline int strcasecmp_ps3(const char *s1, const char *s2) {
    while (*s1 && (tolower((unsigned char)*s1) == tolower((unsigned char)*s2))) {
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

static inline int strncasecmp_ps3(const char *s1, const char *s2, size_t n) {
    if (n == 0) return 0;
    while (n-- > 0 && *s1 && (tolower((unsigned char)*s1) == tolower((unsigned char)*s2))) {
        if (n == 0) return 0;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

#ifdef __cplusplus
}
#endif

#if defined(__PS3__) || defined(PS3) || defined(__CELLOS_LV2__)
#include <PSGL/psgl.h>
#include <PSGL/psglu.h>

#define glDeleteTextures psglDeleteTextures
#define glGenTextures psglGenTextures
#define glBindTexture psglBindTexture
#define glTexParameterf psglTexParameterf
#define glTexImage2D psglTexImage2D
#define glTexSubImage2D psglTexSubImage2D
#define glClearColor psglClearColor
#define glDisable psglDisable
#define glEnable psglEnable
#define glBlendFunc psglBlendFunc
#define glMatrixMode psglMatrixMode
#define glLoadIdentity psglLoadIdentity
#define glLightfv psglLightfv
#define glViewport psglViewport
#define glScalef psglScalef
#define glMultMatrixf psglMultMatrixf
#define glClear psglClear
#define glEnableClientState psglEnableClientState
#define glLoadMatrixf psglLoadMatrixf
#define glVertexPointer psglVertexPointer
#define glTexCoordPointer psglTexCoordPointer
#define glDisableClientState psglDisableClientState
#define glColorPointer psglColorPointer
#define glNormalPointer psglNormalPointer
#define glDrawElements psglDrawElements
#define glPushMatrix psglPushMatrix
#define glPopMatrix psglPopMatrix
#define glGetIntegerv psglGetIntegerv
#endif

// Some SDK versions might have these with a slightly different name or in different paths
#ifndef SCE_PS3_SDK
#define SCE_PS3_SDK "C:/usr/local/cell"
#endif

#ifndef __cplusplus
#ifndef nullptr
#define nullptr NULL
#endif
#endif

#if defined(__cplusplus) && __cplusplus < 201103L
#ifndef nullptr
#define nullptr 0
#endif
#endif

// Missing from some PS3 SDK versions or need specific includes
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef _OGG_CONFIG_TYPES_H
#define _OGG_CONFIG_TYPES_H
#endif

#ifndef OV_FALSE
#define OV_FALSE      -1
#endif
#ifndef OV_EOF
#define OV_EOF        -2
#endif
#ifndef OV_HOLE
#define OV_HOLE       -3
#endif

#ifndef OV_EREAD
#define OV_EREAD      -128
#endif
#ifndef OV_EFAULT
#define OV_EFAULT     -129
#endif
#ifndef OV_EIMPL
#define OV_EIMPL      -130
#endif
#ifndef OV_EINVAL
#define OV_EINVAL     -131
#endif
#ifndef OV_ENOTVORBIS
#define OV_ENOTVORBIS -132
#endif
#ifndef OV_EBADHEADER
#define OV_EBADHEADER -133
#endif
#ifndef OV_EVERSION
#define OV_EVERSION   -134
#endif
#ifndef OV_ENOTAUDIO
#define OV_ENOTAUDIO  -135
#endif
#ifndef OV_EBADPACKET
#define OV_EBADPACKET -136
#endif
#ifndef OV_EBADLINK
#define OV_EBADLINK   -137
#endif
#ifndef OV_ENOSEEK
#define OV_ENOSEEK    -138
#endif

#endif // PS3

#endif // PS3_COMPAT_H
