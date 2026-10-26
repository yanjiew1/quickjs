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
#include "../icu4c/helpers.h"
#include "../../src/intl/icu4x/datetime-data.h"
#include "../../src/intl/datetime/provider.h"

static uint32_t locale(const QJSIntlData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes tag;
        uint32_t parent;
        assert(!qjs_intl_data_locale(data, i, &tag, &parent));
        if (tag.length == strlen(name) && !memcmp(tag.data, name, tag.length))
            return i;
    }
    abort();
}

int main(void)
{
    static const char *const kinds[] = {"generic", "standard", "daylight"};
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSIntlData data;
    QJSIntlTimeZoneNameData names;
    QJSIntlTimeZonePresentationData presentation;
    QJSIntlDateZoneData binding = {0}, initial;
    void *owner = NULL;
    uint32_t group;
    size_t calls, i, public_count = 0;
    assert(!qjs_intl_data_open(qjs_icu4x_datetime_bytes, qjs_icu4x_datetime_length, &allocator,
                               &data));
    assert(!qjs_intl_timezone_name_data_open(&data, &allocator, &names));
    assert(!qjs_intl_timezone_presentation_data_open(&data, &allocator, &presentation));
    binding.names = &names;
    binding.presentation = &presentation;
    binding.names_locale = locale(&data, "en");
    initial = binding;
    calls = state.calls;
    assert(!qjs_intl_datetime_provider_zone_open(&allocator, &binding, &owner));
    calls = state.calls - calls;
    assert(owner && binding.materialize);
    /* Traverse real resolved origins, including aliases. Matching public spans
       must be retained independently of the original companion leaf. */
    for (group = 0; group < names.group_count; group++) {
        QJSIntlTimeZoneNameGroup key;
        uint32_t head;
        unsigned kind;
        assert(!qjs_intl_timezone_name_group_at(&names, group, &key));
        if (key.family < QJS_INTL_TIMEZONE_NAME_ZONE)
            continue;
        assert(!qjs_intl_timezone_name_head(&names, binding.names_locale, group, &head));
        for (kind = 0; kind < 3; kind++) {
            QJSIntlTimeZoneNameEntry entry;
            QJSIntlTimeZoneNameNode origin;
            QJSIntlBytes text;
            QJSIntlStatus status = qjs_intl_timezone_name_resolve(
                &names, head, bytes(kinds[kind]), QJS_INTL_TIMEZONE_NAME_NO_TYPE, (QJSIntlBytes){0},
                &entry, &origin);
            if (status == QJS_INTL_UNSUPPORTED ||
                (!status && (entry.blocked || !entry.text.length)))
                continue;
            assert(!status);
            status = binding.materialize(binding.materialize_opaque, &origin, &entry, &text);
            assert(!status || status == QJS_INTL_UNSUPPORTED);
            if (!status) {
                assert(text.length == entry.text.length &&
                       !memcmp(text.data, entry.text.data, text.length));
                assert(text.data != entry.text.data);
                public_count++;
            }
        }
    }
    assert(public_count > 100);
    qjs_intl_datetime_provider_zone_close(owner);
    assert(!state.live);
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        binding = initial;
        owner = (void *)1;
        assert(qjs_intl_datetime_provider_zone_open(&allocator, &binding, &owner) ==
               QJS_INTL_NO_MEMORY);
        assert(!owner && !state.live && !binding.materialize && !binding.materialize_opaque);
        state.fail_at = 0;
    }
    qjs_intl_datetime_provider_zone_close(NULL);
    return 0;
}
