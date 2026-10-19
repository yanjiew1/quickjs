/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NATIVE_WIRE_H
#define QJS_INTL_NATIVE_WIRE_H
#include <stdint.h>

static inline uint32_t qjs_intl_read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
#endif
