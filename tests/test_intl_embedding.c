/* Check compiled prepared-data bytes, not the generator's textual spelling.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../src/intl/native/data.h"
#include <assert.h>

extern const size_t qjs_test_embedding_storage_length;

int main(void)
{
    QJSIntlPack pack;
    QJSIntlSection section;
    size_t i;

    assert(qjs_test_embedding_storage_length == qjs_intl_native_length);
    assert(!qjs_intl_pack_open(qjs_intl_native_bytes, qjs_intl_native_length, &pack));
    assert(!qjs_intl_pack_section(&pack, 123, &section));
    assert(section.count == 1 && section.length == 256 * 17 + 1);
    for (i = 0; i + 1 < section.length; i++)
        assert(section.bytes[i] == i % 256);
    assert(!section.bytes[section.length - 1]);
    return 0;
}
