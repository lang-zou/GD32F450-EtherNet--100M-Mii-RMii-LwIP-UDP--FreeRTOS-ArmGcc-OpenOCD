/**
 * @file    cc.h
 * @brief   LwIP compiler abstraction for ARM GCC (arm-none-eabi-gcc)
 */
#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include "cpu.h"

/* printf format strings for LwIP types */
#define U16_F "hu"
#define S16_F "d"
#define X16_F "hx"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "uz"

/* Packed structures — GCC syntax */
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__ ((__packed__))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

/* Platform assert: delegate to our assert module */
void assert_handler(const char *file, int line, const char *expr);
#define LWIP_PLATFORM_ASSERT(x)  do { if (!(x)) { assert_handler(__FILE__, __LINE__, "LWIP: " #x); } } while (0)

/* Platform diagnostic output */
#define LWIP_PLATFORM_DIAG(x)    do { /* no-op in release */ (void)(x); } while (0)

#endif /* LWIP_ARCH_CC_H */
