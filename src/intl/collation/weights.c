/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "weights.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include <libunicode-table.h>
#include <string.h>

QJSIntlStatus qjs_intl_collation_implicit(uint32_t code, QJSIntlCollationElement out[2])
{
    uint32_t first = 0xfbc0 + (code >> 15), second = (code & 0x7fff) | 0x8000;
    int block;
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, 2 * sizeof(*out));
    if (code > 0x10ffff)
        return QJS_INTL_INVALID_ARGUMENT;
    block = unicode_get_block(code);
    /* Special script families use assigned points only, not the whole block.
       Tangut ideographs and components deliberately have different bases. */
    switch (block) {
    case UNICODE_BLOCK_Tangut:
    case UNICODE_BLOCK_Tangut_Supplement:
        if (lre_is_assigned(code)) {
            first = 0xfb00;
            second = (code - 0x17000) | 0x8000;
        }
        break;
    case UNICODE_BLOCK_Tangut_Components:
    case UNICODE_BLOCK_Tangut_Components_Supplement:
        if (lre_is_assigned(code)) {
            first = 0xfb01;
            second = (code - 0x18800) | 0x8000;
        }
        break;
    case UNICODE_BLOCK_Nushu:
        if (lre_is_assigned(code)) {
            first = 0xfb02;
            second = (code - 0x1b170) | 0x8000;
        }
        break;
    case UNICODE_BLOCK_Khitan_Small_Script:
        if (lre_is_assigned(code)) {
            first = 0xfb03;
            second = (code - 0x18b00) | 0x8000;
        }
        break;
    case UNICODE_BLOCK_Jurchen:
    case UNICODE_BLOCK_Jurchen_Radicals:
        if (lre_is_assigned(code)) {
            first = 0xfb04;
            second = (code - 0x18e00) | 0x8000;
        }
        break;
    case UNICODE_BLOCK_Seal:
        if (lre_is_assigned(code)) {
            first = 0xfb05;
            second = (code - 0x3d000) | 0x8000;
        }
        break;
    default:
        if (lre_is_unified_ideograph(code)) {
            int core = block == UNICODE_BLOCK_CJK_Unified_Ideographs ||
                       block == UNICODE_BLOCK_CJK_Compatibility_Ideographs;
            first = (core ? 0xfb40 : 0xfb80) + (code >> 15);
        }
        break;
    }
    out[0].primary = first;
    out[0].secondary = 0x20;
    out[0].tertiary = 2;
    out[1].primary = second;
    return QJS_INTL_OK;
}
