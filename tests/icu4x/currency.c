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
#include "../../src/icu4x/currency.h"
#include "../../src/icu4x/names.h"
#include <assert.h>
#include <string.h>

typedef struct Inventory {
    char previous[4];
    size_t count, calls;
    int usd, eur, xad, unknown, error;
} Inventory;

static int code(void *opaque, const uint8_t *text, size_t length)
{
    Inventory *out = opaque;
    size_t i;

    out->calls++;
    if (out->error)
        return out->error;
    assert(length == 3);
    for (i = 0; i < 3; i++)
        assert(text[i] >= 'A' && text[i] <= 'Z');
    if (out->count)
        assert(memcmp(out->previous, text, 3) < 0);
    memcpy(out->previous, text, 3);
    out->count++;
    out->usd |= !memcmp(text, "USD", 3);
    out->eur |= !memcmp(text, "EUR", 3);
    out->xad |= !memcmp(text, "XAD", 3);
    out->unknown |= !memcmp(text, "ZZZ", 3);
    return 0;
}

typedef struct Output {
    uint16_t text[128];
    size_t length, calls;
    int error;
} Output;

static int write_text(void *opaque, const uint16_t *text, size_t length)
{
    Output *out = opaque;
    out->calls++;
    if (out->error)
        return out->error;
    assert(length <= sizeof(out->text) / sizeof(*out->text) - out->length);
    if (length)
        memcpy(out->text + out->length, text, length * sizeof(*text));
    out->length += length;
    return 0;
}

int main(void)
{
    Inventory inventory = {0}, failed = {.error = QJS_ICU4X_MEMORY};
    Output output = {0};
    QJSICU4XNames *names = NULL;
    QJSICU4XTextSink sink = {&output, write_text};
    static const char expected[] = "US Dollar";
    size_t i;

    assert(!qjs_icu4x_currency_names(&inventory, code));
    assert(inventory.count > 200 && inventory.usd && inventory.eur && !inventory.unknown);
    // XAD has a CLDR plain name in Chuvash, even though ISO fraction metadata
    // can omit it. A symbols/metadata union would give the wrong inventory.
    assert(inventory.xad);
    assert(qjs_icu4x_currency_names(&failed, code) == QJS_ICU4X_MEMORY);
    assert(failed.calls == 1 && !failed.count);
    assert(!qjs_icu4x_names_open((const uint8_t *)"en", 2, 0, 1, &names));
    assert(!qjs_icu4x_names_format(names, 3, (const uint8_t *)"USD", 3, sink));
    assert(output.length == strlen(expected));
    for (i = 0; i < output.length; i++)
        assert(output.text[i] == (unsigned char)expected[i]);
    qjs_icu4x_names_close(names);
    output = (Output){0};
    // en-US has no separate plain-name row here; public marker fallback must
    // keep the currency attribute while resolving the parent locale.
    assert(!qjs_icu4x_names_open((const uint8_t *)"en-US", 5, 0, 1, &names));
    assert(!qjs_icu4x_names_format(names, 3, (const uint8_t *)"USD", 3, sink));
    assert(output.length == strlen(expected));
    for (i = 0; i < output.length; i++)
        assert(output.text[i] == (unsigned char)expected[i]);
    output = (Output){0};
    assert(qjs_icu4x_names_format(names, 3, (const uint8_t *)"ZZZ", 3, sink) ==
           QJS_ICU4X_UNSUPPORTED);
    assert(!output.calls && !output.length);
    assert(qjs_icu4x_names_format(names, 3, (const uint8_t *)"usd", 3, sink) == QJS_ICU4X_RANGE);
    assert(!output.calls);
    output.error = QJS_ICU4X_MEMORY;
    assert(qjs_icu4x_names_format(names, 3, (const uint8_t *)"USD", 3, sink) == QJS_ICU4X_MEMORY);
    assert(output.calls == 1 && !output.length);
    qjs_icu4x_names_close(names);
    output = (Output){0};
    assert(!qjs_icu4x_names_open((const uint8_t *)"cv", 2, 0, 1, &names));
    assert(!qjs_icu4x_names_format(names, 3, (const uint8_t *)"XAD", 3, sink));
    assert(output.length);
    qjs_icu4x_names_close(names);
    return 0;
}
