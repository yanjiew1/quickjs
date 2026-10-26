/* ICU4X-owned exact plural adapter shared by ICU provider composition.
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
#include "snapshot-icu4x.h"
#include "../icu4x/common.h"
#include <string.h>

static int compare(QJSIntlBytes tag, const char *source)
{
    size_t length = strlen(source);
    int order = memcmp(tag.data, source, tag.length < length ? tag.length : length);
    return order ? order : tag.length < length ? -1 : tag.length != length;
}

QJSIntlStatus qjs_intl_icu4x_plural_open(QJSIntlBytes locale, unsigned int ordinal,
                                         QJSICU4XPlural **out, QJSICU4XPluralData *result)
{
    QJSICU4XPluralData data;
    QJSICU4XPlural *handle = NULL;
    const QJSICU4XPluralFamily *family = NULL;
    QJSIntlBytes rules = locale, ranges = locale;
    QJSIntlStatus status;
    size_t low = 0, high = qjs_icu4x_plural_source_count;
    if (out)
        *out = NULL;
    if (!out || !result || !locale.data || !locale.length || ordinal > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const QJSICU4XPluralSource *source = &qjs_icu4x_plural_sources[middle];
        int order = compare(locale, source->locale);
        if (order < 0)
            high = middle;
        else if (order > 0)
            low = middle + 1;
        else {
            rules = (QJSIntlBytes){source->rule_locales[ordinal],
                                   strlen(source->rule_locales[ordinal])};
            ranges = (QJSIntlBytes){source->range_locale, strlen(source->range_locale)};
            family = &qjs_icu4x_plural_families[source->families[ordinal]];
            break;
        }
    }
    status = qjs_icu4x_status(qjs_icu4x_plural_check_version(
        (const uint8_t *)qjs_icu4x_plural_source_version, strlen(qjs_icu4x_plural_source_version)));
    if (!status)
        status = qjs_icu4x_status(qjs_icu4x_plural_open_sources(
            (const uint8_t *)rules.data, rules.length, (const uint8_t *)ranges.data, ranges.length,
            ordinal, &handle));
    if (!status)
        status = qjs_icu4x_status(qjs_icu4x_plural_data(handle, &data));
    if (status) {
        qjs_icu4x_plural_close(handle);
        return status;
    }
    if (family) {
        /* Source inheritance precedes lookup: en-Dsrt uses public en rules,
           not the root payload returned for an unsupported script row. The
           remaining missing languages (such as gv) need matching CLDR rules. */
        if ((data.mask & family->mask) != family->mask) {
            unsigned i;
            data.mask = family->mask;
            for (i = 0; i < 6; i++) {
                const char *rule = family->rules[i];
                data.rules[i] = (QJSICU4XRuleBytes){(const uint8_t *)rule, rule ? strlen(rule) : 0};
            }
        }
        /* Ranges have their own source; public omissions must not change a
           declared pair to the end-default. Keep complete public vectors. */
        if (memcmp(data.ranges, family->ranges, sizeof(data.ranges)))
            memcpy(data.ranges, family->ranges, sizeof(data.ranges));
    }
    *result = data;
    *out = handle;
    return QJS_INTL_OK;
}
