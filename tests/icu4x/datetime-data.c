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
#include "../../src/intl/datetime/calendar-identity.h"
#include "../../src/intl/datetime/era-data.h"
#include "../../src/intl/datetime/dayperiod-data.h"
#include "../../src/intl/datetime/timezone-name-data.h"
#include "../../src/intl/datetime/timezone-presentation-data.h"
#include "../../src/intl/display/data.h"
#include "../../src/intl/locale/preference-data.h"
#include "../../src/intl/number/symbol-data.h"

static QJSIntlStatus admit(const QJSIntlAllocator *allocator)
{
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlNameData names;
    uint32_t locale, group, head;
    QJSIntlNumberSymbolData symbols;
    QJSIntlSection absent;
    QJSIntlStatus status;
    status =
        qjs_intl_data_open(qjs_icu4x_datetime_bytes, qjs_icu4x_datetime_length, allocator, &data);
    if (status)
        return status;
    /* This companion cannot silently supply unrelated native Intl providers. */
    assert(qjs_intl_pack_section(&data.pack, QJS_INTL_SECTION_PLURALS, &absent) ==
           QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_pack_section(&data.pack, QJS_INTL_SECTION_UNIT_PATTERNS, &absent) ==
           QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_pack_section(&data.pack, QJS_INTL_SECTION_LISTS, &absent) ==
           QJS_INTL_UNSUPPORTED);
#define OPEN(TYPE, FUNCTION)                                                                       \
    do {                                                                                           \
        TYPE view;                                                                                 \
        status = FUNCTION(&data, allocator, &view);                                                \
        if (status)                                                                                \
            return status;                                                                         \
    } while (0)
    OPEN(QJSIntlLocaleMetadata, qjs_intl_locale_metadata_open);
    OPEN(QJSIntlLocalePreferences, qjs_intl_locale_preferences_open);
    OPEN(QJSIntlDateAvailableData, qjs_intl_date_available_data_open);
    OPEN(QJSIntlDateCombinationData, qjs_intl_date_combination_data_open);
    OPEN(QJSIntlDateStyleData, qjs_intl_date_style_data_open);
    OPEN(QJSIntlDateIntervalData, qjs_intl_date_interval_data_open);
    OPEN(QJSIntlCalendarNameData, qjs_intl_calendar_name_data_open);
    OPEN(QJSIntlCalendarEraData, qjs_intl_calendar_era_data_open);
    OPEN(QJSIntlDayPeriodData, qjs_intl_dayperiod_data_open);
    OPEN(QJSIntlTimeZoneNameData, qjs_intl_timezone_name_data_open);
    OPEN(QJSIntlTimeZonePresentationData, qjs_intl_timezone_presentation_data_open);
#undef OPEN
    status = qjs_intl_name_data_open(&data, allocator, &names);
    if (status)
        return status;
    for (locale = 0; locale < data.locales.count; locale++) {
        for (group = 0; group < QJS_INTL_NAME_GROUP_COUNT; group++) {
            if (group == QJS_INTL_NAME_REGION)
                continue;
            assert(!qjs_intl_name_head(&names, locale, (QJSIntlNameGroup)group, &head));
            assert(!head);
        }
    }
    status = qjs_intl_numbering_data_open(&data, allocator, &numbering);
    if (!status)
        status = qjs_intl_number_symbol_data_open(&numbering, allocator, &symbols);
    return status;
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    size_t i, calls;
    assert(!strcmp(qjs_icu4x_datetime_version, "49.0.0"));
    assert(!admit(&allocator));
    assert(!state.live);
    calls = state.calls;
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        assert(admit(&allocator) == QJS_INTL_NO_MEMORY);
        assert(!state.live);
    }
    return 0;
}
