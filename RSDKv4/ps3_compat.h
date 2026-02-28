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

#ifndef OV_EREAD
#define OV_EREAD      -128
#endif
#ifndef OV_EFAULT
#define OV_EFAULT     -129
#endif
#ifndef OV_EVERSION
#define OV_EVERSION   -130
#endif
#ifndef OV_EBADHEADER
#define OV_EBADHEADER -131
#endif
#ifndef OV_ENOTVORBIS
#define OV_ENOTVORBIS -132
#endif
#ifndef OV_EBADPACKET
#define OV_EBADPACKET -133
#endif
#ifndef OV_EBADLINK
#define OV_EBADLINK   -134
#endif
#ifndef OV_ENOSEEK
#define OV_ENOSEEK    -135
#endif

#endif // PS3

#endif // PS3_COMPAT_H
