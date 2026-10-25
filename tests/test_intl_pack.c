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
#include "../src/intl/native/pack.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put_u32(uint8_t *bytes, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        bytes[i] = value >> (i * 8);
}

/* Same authored layout as test_intl_bundle.py, independent of host endian. */
static void fixture(uint8_t *bytes)
{
    unsigned int i;
    memset(bytes, 0, 105);
    memcpy(bytes, "QJIN", 4);
    bytes[4] = 1;
    put_u32(bytes + 8, 105);
    put_u32(bytes + 12, 2);
    for (i = 0; i < 32; i++)
        bytes[16 + i] = i;
    put_u32(bytes + 48, 1);
    put_u32(bytes + 64, 2);
    put_u32(bytes + 68, 96);
    put_u32(bytes + 72, 3);
    put_u32(bytes + 76, 1);
    put_u32(bytes + 80, 10);
    put_u32(bytes + 84, 100);
    put_u32(bytes + 88, 5);
    put_u32(bytes + 92, 3);
    memcpy(bytes + 96, "abc", 3);
    memcpy(bytes + 100, "xyz12", 5);
}

int main(void)
{
    uint8_t storage[107], *bytes = storage + 1;
    QJSIntlPack pack;
    QJSIntlSection section;
    size_t i;
    static const struct { size_t offset; uint32_t value; } invalid[] = {
        {8, 106}, {12, 0}, {12, 129}, {48, 2}, {52, 1}, {56, 1},
        {60, 1}, {64, 0}, {80, 2}, {68, 0xfffffffc}, {72, 0xffffffff},
        {84, 104}, {84, 96}, {84, 99},
    };

    fixture(bytes);
    assert(qjs_intl_pack_open(bytes, 105, &pack) == QJS_INTL_OK);
    assert(pack.profile == 1 && pack.section_count == 2);
    assert(pack.generation == bytes + 16 && pack.generation[31] == 31);
    assert(qjs_intl_pack_section(&pack, 2, &section) == QJS_INTL_OK);
    assert(section.length == 3 && section.count == 1);
    assert(!memcmp(section.bytes, "abc", 3));
    assert(qjs_intl_pack_section(&pack, 10, &section) == QJS_INTL_OK);
    assert(section.length == 5 && section.count == 3);
    assert(!memcmp(section.bytes, "xyz12", 5));
    assert(qjs_intl_pack_section(&pack, 3, &section) == QJS_INTL_UNSUPPORTED);
    assert(!section.bytes && !section.length && !section.count);
    assert(qjs_intl_pack_open(bytes, 106, &pack) == QJS_INTL_DATA_ERROR);
    assert(qjs_intl_pack_open(bytes, 256 * 1024 * 1024 + 1, &pack) == QJS_INTL_DATA_ERROR);
    for (i = 0; i < 105; i++) {
        assert(qjs_intl_pack_open(bytes, i, &pack) == QJS_INTL_DATA_ERROR);
        assert(!pack.bytes && !pack.length && !pack.generation);
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        fixture(bytes);
        put_u32(bytes + invalid[i].offset, invalid[i].value);
        assert(qjs_intl_pack_open(bytes, 105, &pack) == QJS_INTL_DATA_ERROR);
        assert(!pack.bytes);
    }
    fixture(bytes);
    bytes[99] = 1;
    assert(qjs_intl_pack_open(bytes, 105, &pack) == QJS_INTL_DATA_ERROR);
    fixture(bytes);
    bytes[4] = 2;
    assert(qjs_intl_pack_open(bytes, 105, &pack) == QJS_INTL_UNSUPPORTED);
    fixture(bytes);
    bytes[6] = 1;
    assert(qjs_intl_pack_open(bytes, 105, &pack) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_pack_open(NULL, 0, &pack) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_pack_open(bytes, 105, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_pack_section(NULL, 1, &section) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_pack_section(&pack, 1, NULL) == QJS_INTL_INVALID_ARGUMENT);
    fixture(bytes);
    put_u32(bytes + 8, 80);
    put_u32(bytes + 12, 1);
    put_u32(bytes + 48, 0);
    put_u32(bytes + 64, 23);
    put_u32(bytes + 68, 80);
    put_u32(bytes + 72, 0);
    put_u32(bytes + 76, 0);
    assert(qjs_intl_pack_open(bytes, 80, &pack) == QJS_INTL_OK);
    assert(pack.profile == 0);
    assert(qjs_intl_pack_section(&pack, 23, &section) == QJS_INTL_OK);
    assert(section.bytes == bytes + 80 && !section.length && !section.count);
    puts("Intl pack envelope tests passed");
    return 0;
}
