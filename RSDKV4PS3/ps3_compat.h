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
#include <sys/ppu_thread.h>
#include <sys/synchronization.h>

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
#define PSGL_USE_GL_NAMES
#include <PSGL/psgl.h>
#include <PSGL/psglu.h>

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

#endif // PS3

#endif // PS3_COMPAT_H
