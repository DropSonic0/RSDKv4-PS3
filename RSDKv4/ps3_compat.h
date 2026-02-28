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

#define ov_pcm_tell psgl_ov_pcm_tell
#define ov_open_callbacks psgl_ov_open_callbacks
#define ov_info psgl_ov_info
#define ov_pcm_total psgl_ov_pcm_total
#define ov_pcm_seek psgl_ov_pcm_seek
#define ov_clear psgl_ov_clear
#define ov_read psgl_ov_read
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

#define _OGG_CONFIG_TYPES_H

#define OV_FALSE      -1
#define OV_EOF        -2
#define OV_HOLE       -3

#define OV_EREAD      -128
#define OV_EFAULT     -129
#define OV_EIMPL      -130
#define OV_EINVAL     -131
#define OV_ENOTVORBIS -132
#define OV_EBADHEADER -133
#define OV_EVERSION   -134
#define OV_ENOTAUDIO  -135
#define OV_EBADPACKET -136
#define OV_EBADLINK   -137
#define OV_ENOSEEK    -138

#endif // PS3

#endif // PS3_COMPAT_H
