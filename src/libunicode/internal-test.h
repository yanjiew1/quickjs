/*
 * Unicode utilities private generator self-test interfaces
 *
 * Copyright (c) 2017-2018 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef LIBUNICODE_INTERNAL_TEST_H
#define LIBUNICODE_INTERNAL_TEST_H

#include "../../libunicode.h"
#include "../../cutils.h"

#if defined(__GNUC__) || defined(__clang__)
#define LIBUNICODE_TEST_INTERNAL __attribute__((visibility("hidden")))
#else
#define LIBUNICODE_TEST_INTERNAL
#endif

#ifdef CONFIG_UNICODE_TEST
LIBUNICODE_TEST_INTERNAL int qjs_unicode_test_decomp_char(
    uint32_t *res, uint32_t c, BOOL is_compat);
LIBUNICODE_TEST_INTERNAL int qjs_unicode_test_compose_pair(
    uint32_t c0, uint32_t c1);
LIBUNICODE_TEST_INTERNAL int qjs_unicode_test_get_cc(uint32_t c);
#endif

#endif /* LIBUNICODE_INTERNAL_TEST_H */
