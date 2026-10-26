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
#include "../../src/intl/datetime/calendar-identity.h"

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

static void lookup(QJSIntlDateTextData *binding, unsigned symbol, size_t width, const char *code,
                   unsigned leap_year, const QJSIntlDateCalendarFields *local, int require_public)
{
    QJSIntlDateNameSelection selected;
    QJSIntlDatePatternToken token = {0};
    QJSIntlBytes result = {0};
    QJSIntlStatus status;
    assert(!qjs_intl_date_name_select(binding->names, binding->locale, binding->calendar, symbol,
                                      width, bytes(code), leap_year, &selected));
    assert(selected.state == QJS_INTL_DATE_NAME_TEXT);
    token.symbol = symbol;
    token.width = width;
    status = binding->materialize(binding->materialize_opaque, local, &token, &selected, &result);
    assert(status == QJS_INTL_OK || status == QJS_INTL_UNSUPPORTED);
    if (require_public)
        assert(!status);
    if (!status) {
        assert(result.length == selected.entry.text.length &&
               !memcmp(result.data, selected.entry.text.data, result.length));
        assert(result.data != selected.entry.text.data);
    }
}

static void template_lookup(QJSIntlDateTextData *binding, const QJSIntlDateCalendarFields *local)
{
    QJSIntlCalendarNameGroup group = {0};
    QJSIntlDateNameSelection selected = {0};
    QJSIntlDatePatternToken token = {0};
    QJSIntlBytes result;
    uint32_t index, head;
    group.calendar = binding->calendar;
    group.family = QJS_INTL_CAL_NAME_MONTH_PATTERN;
    group.context = QJS_INTL_CAL_NAME_NUMERIC;
    group.width = QJS_INTL_CAL_NAME_ALL;
    assert(!qjs_intl_calendar_name_find_group(binding->names, &group, &index));
    assert(!qjs_intl_calendar_name_head(binding->names, binding->locale, index, &head));
    assert(!qjs_intl_calendar_name_resolve(binding->names, head, bytes("leap"),
                                           QJS_INTL_CAL_NAME_NO_YEAR, (QJSIntlBytes){0},
                                           &selected.entry, &selected.origin));
    assert(!selected.entry.blocked && selected.entry.text.length);
    selected.state = QJS_INTL_DATE_NAME_TEXT;
    token.symbol = 'M';
    token.width = 1;
    assert(!binding->materialize(binding->materialize_opaque, local, &token, &selected, &result));
    assert(result.length == selected.entry.text.length &&
           !memcmp(result.data, selected.entry.text.data, result.length));
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSIntlData data;
    QJSIntlCalendarNameData names;
    QJSIntlCalendarEraData eras;
    QJSIntlLocaleMetadata metadata;
    QJSIntlDateTextData binding = {0}, initial;
    QJSIntlDateCalendarFields local = {0};
    void *owner = NULL;
    size_t calls, i;
    assert(!qjs_intl_data_open(qjs_icu4x_datetime_bytes, qjs_icu4x_datetime_length, &allocator,
                               &data));
    assert(!qjs_intl_calendar_name_data_open(&data, &allocator, &names));
    assert(!qjs_intl_calendar_era_data_open(&data, &allocator, &eras));
    assert(!qjs_intl_locale_metadata_open(&data, &allocator, &metadata));
    assert(!state.live);
    binding.names = &names;
    binding.metadata = &metadata;
    binding.eras = &eras;
    binding.locale = locale(&data, "en");
    assert(
        !qjs_intl_date_name_calendar_index(&metadata, &names, bytes("gregory"), &binding.calendar));
    initial = binding;
    calls = state.calls;
    assert(!qjs_intl_datetime_provider_text_open(&allocator, &binding, QJS_CAL_GREGORY, &owner));
    calls = state.calls - calls;
    assert(owner && binding.materialize);
    strcpy(local.month_key, "1");
    lookup(&binding, 'M', 4, "1", 0, &local, 1);
    lookup(&binding, 'E', 4, "sun", 0, &local, 1);
    local.has_era = 1;
    local.era_calendar = QJS_CAL_GREGORY;
    strcpy(local.era_code, "ce");
    lookup(&binding, 'G', 4, "1", 0, &local, 1);
    qjs_intl_datetime_provider_text_close(owner);
    assert(!state.live);
    for (i = 1; i <= calls; i++) {
        binding = initial;
        state.fail_at = state.calls + i;
        assert(qjs_intl_datetime_provider_text_open(&allocator, &binding, QJS_CAL_GREGORY,
                                                    &owner) == QJS_INTL_NO_MEMORY);
        assert(!owner && !binding.materialize && !binding.materialize_opaque && !state.live);
    }
    state.fail_at = 0;
    binding = initial;
    binding.locale = locale(&data, "ru");
    assert(!qjs_intl_datetime_provider_text_open(&allocator, &binding, QJS_CAL_GREGORY, &owner));
    lookup(&binding, 'M', 4, "1", 0, &local, 1);
    lookup(&binding, 'L', 4, "1", 0, &local, 1);
    qjs_intl_datetime_provider_text_close(owner);
    binding = initial;
    assert(
        !qjs_intl_date_name_calendar_index(&metadata, &names, bytes("hebrew"), &binding.calendar));
    assert(!qjs_intl_datetime_provider_text_open(&allocator, &binding, QJS_CAL_HEBREW, &owner));
    local.in_leap_year = 1;
    lookup(&binding, 'M', 4, "6", 1, &local, 1);
    lookup(&binding, 'M', 4, "7", 1, &local, 1);
    local.in_leap_year = 0;
    lookup(&binding, 'M', 4, "7", 0, &local, 1);
    qjs_intl_datetime_provider_text_close(owner);
    binding = initial;
    assert(
        !qjs_intl_date_name_calendar_index(&metadata, &names, bytes("chinese"), &binding.calendar));
    assert(!qjs_intl_datetime_provider_text_open(&allocator, &binding, QJS_CAL_CHINESE, &owner));
    template_lookup(&binding, &local);
    qjs_intl_datetime_provider_text_close(owner);
    qjs_intl_datetime_provider_text_close(NULL);
    assert(!state.live);
    return 0;
}
