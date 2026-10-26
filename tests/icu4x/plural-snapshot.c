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
#include "../../src/intl/plural/snapshot-icu4x.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    size_t i, supplemented = 0, inherited = 0;
    unsigned kind, category;
    /* Inspect real compiled source rows through the actual C/Rust boundary.
       The selected library remains the owner when its rule family is complete. */
    for (i = 0; i < qjs_icu4x_plural_source_count; i++) {
        const QJSICU4XPluralSource *source = &qjs_icu4x_plural_sources[i];
        QJSIntlBytes locale = {source->locale, strlen(source->locale)};
        for (kind = 0; kind < 2; kind++) {
            const QJSICU4XPluralFamily *family = &qjs_icu4x_plural_families[source->families[kind]];
            QJSICU4XPlural *handle = NULL;
            QJSICU4XPluralData data, raw;
            assert(!qjs_intl_icu4x_plural_open(locale, kind, &handle, &data));
            assert(!qjs_icu4x_plural_data(handle, &raw));
            assert(data.mask == family->mask);
            assert(!memcmp(data.ranges, family->ranges, sizeof(data.ranges)));
            if ((raw.mask & family->mask) == family->mask) {
                for (category = 0; category < 6; category++)
                    assert(data.rules[category].data == raw.rules[category].data);
                if (strcmp(source->locale, source->rule_locales[kind]))
                    inherited++;
            } else {
                supplemented++;
            }
            qjs_icu4x_plural_close(handle);
        }
    }
    assert(supplemented && inherited);
    return 0;
}
