/* Shared size-option parser for qjs and qjsc.
 * Copyright (c) 2026 Yan-Jie Wang
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
#ifndef QUICKJS_TOOLS_SIZE_OPTION_H
#define QUICKJS_TOOLS_SIZE_OPTION_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static inline size_t js_parse_size_option(const char *text, const char *program)
{
    const unsigned char *p = (const unsigned char *)text;
    size_t value = 0;
    unsigned shift = 0;
    unsigned digit;

    if (*p == '+')
        p++;
    if (*p < '0' || *p > '9')
        goto invalid;
    do {
        digit = *p++ - '0';
        if (value > (SIZE_MAX - digit) / 10)
            goto invalid;
        value = value * 10 + digit;
    } while (*p >= '0' && *p <= '9');

    if (*p) {
        switch (*p++) {
        case 'G':
            shift = 30;
            break;
        case 'M':
            shift = 20;
            break;
        case 'k':
        case 'K':
            shift = 10;
            break;
        default:
            goto invalid;
        }
        if (*p)
            goto invalid;
    }
    if (value > (SIZE_MAX >> shift))
        goto invalid;
    return value << shift;

invalid:
    fprintf(stderr, "%s: invalid size: %s\n", program, text);
    exit(1);
}

#endif
