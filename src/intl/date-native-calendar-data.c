/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * One linear data projection into bounded stack masks. No allocation,
 * retained data slices or second calendar/arithmetic owner.
 */
#include "date-native-data.h"
#include "data/date-data-validation.h"
#include <string.h>

typedef struct CalendarProof {
    unsigned int styles, fallback, present;
    uint16_t months[2][3];
    uint8_t weekdays[2][4], periods[2][3], eras[3], leap_named, leap_numeric;
    uint64_t cyclic[3];
} CalendarProof;

static int calendar_id(const QJSIntlDataView *v, const QJSIntlDataSection *s,
    uint32_t row, QJSCalendarId *id)
{
    QJSIntlDataSlice key;
    const char *canonical;
    if (qjs_intl_data_record_string(v, s, row, 4, &key) ||
        qjs_calendar_from_identifier(id, (const char *)key.data, key.length)) return 0;
    canonical = qjs_calendar_identifier(*id);
    return key.length == strlen(canonical) && !memcmp(key.data, canonical, key.length);
}
static int era_slot(QJSCalendarId id, uint32_t index)
{
    if (id == QJS_CAL_JAPANESE) return index >= 232 && index <= 238 ? (int)index - 232 : -1;
    if (id == QJS_CAL_COPTIC) return index == 1 ? 0 : -1;
    return index <= 1 ? (int)index : -1;
}
QJSIntlStatus qjs_intl_native_date_data_calendar_mask(const QJSIntlDataView *v,
    uint32_t locale, uint32_t *out)
{
    QJSIntlDataSection patterns, names, fallbacks, locales;
    CalendarProof proof[QJS_CAL_COUNT];
    uint32_t mask = 0, i, l, index;
    unsigned int id, context, width;
    QJSCalendarId calendar;
    QJSIntlDataSlice row;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = 0;
    if (!v) return QJS_INTL_INVALID_ARGUMENT;
    if (qjs_intl_date_data_validate(v) || qjs_intl_data_section(v, 16, &locales)) return QJS_INTL_DATA_ERROR;
    if (locale >= locales.record_count) return QJS_INTL_INVALID_ARGUMENT;
    if (qjs_intl_data_section(v, 100, &patterns) || qjs_intl_data_section(v, 101, &names) ||
        qjs_intl_data_section(v, 107, &fallbacks)) return QJS_INTL_UNSUPPORTED;
    memset(proof, 0, sizeof(proof));
    for (i = 0; i < patterns.record_count; i++) {
        if (qjs_intl_data_record_u32(&patterns, i, 0, &l) ||
            qjs_intl_data_record(&patterns, i, &row)) return QJS_INTL_DATA_ERROR;
        if (l != locale || !calendar_id(v, &patterns, i, &calendar)) continue;
        proof[calendar].present = 1;
        if (row.data[12] >= 1 && row.data[12] <= 3)
            proof[calendar].styles |= 1u << ((row.data[12] - 1) * 4 + row.data[13]);
    }
    for (i = 0; i < fallbacks.record_count; i++) {
        if (qjs_intl_data_record_u32(&fallbacks, i, 0, &l)) return QJS_INTL_DATA_ERROR;
        if (l == locale && calendar_id(v, &fallbacks, i, &calendar)) proof[calendar].fallback = 1;
    }
    for (i = 0; i < names.record_count; i++) {
        CalendarProof *p;
        if (qjs_intl_data_record_u32(&names, i, 0, &l) ||
            qjs_intl_data_record_u32(&names, i, 16, &index) ||
            qjs_intl_data_record(&names, i, &row)) return QJS_INTL_DATA_ERROR;
        if (l != locale || !calendar_id(v, &names, i, &calendar)) continue;
        p = &proof[calendar]; context = row.data[13]; width = row.data[14];
        switch (row.data[12]) {
        case 0: {
            int slot = era_slot(calendar, index);
            if (slot >= 0) p->eras[width] |= (uint8_t)(1u << slot);
            break;
        }
        case 1: p->months[context][width] |= (uint16_t)(1u << (index - 1)); break;
        case 2: p->weekdays[context][width] |= (uint8_t)(1u << (index - 1)); break;
        case 3:
            if (index < 2) p->periods[context][width] |= (uint8_t)(1u << index);
            break;
        case 4: p->cyclic[width] |= UINT64_C(1) << (index - 1); break;
        case 5:
            if (index) p->leap_named |= (uint8_t)(1u << (context * 3 + width));
            else p->leap_numeric = 1;
            break;
        default: return QJS_INTL_DATA_ERROR;
        }
    }
    for (id = 0; id < QJS_CAL_COUNT; id++) {
        const CalendarProof *p = &proof[id];
        unsigned int complete = p->styles == 0xfff && p->fallback;
        unsigned int months = id == QJS_CAL_HEBREW ? 14 :
            id == QJS_CAL_COPTIC || id == QJS_CAL_ETHIOAA || id == QJS_CAL_ETHIOPIC ? 13 : 12;
        uint16_t month_bits = (uint16_t)((1u << months) - 1);
        unsigned int era_bits = id == QJS_CAL_JAPANESE ? 0x7f :
            id == QJS_CAL_GREGORY || id == QJS_CAL_ISO8601 || id == QJS_CAL_ETHIOPIC ||
            id == QJS_CAL_ROC || id == QJS_CAL_ISLAMIC_CIVIL || id == QJS_CAL_ISLAMIC_TBLA ||
            id == QJS_CAL_ISLAMIC_UMALQURA ? 3 : 1;
        if (!complete) continue;
        for (context = 0; context < 2; context++) {
            for (width = 0; width < 3; width++)
                if ((p->months[context][width] & month_bits) != month_bits ||
                    p->weekdays[context][width] != 0x7f || p->periods[context][width] != 3) complete = 0;
            if (p->weekdays[context][3] != 0x7f) complete = 0;
        }
        for (width = 0; width < 3; width++) {
            if (id == QJS_CAL_CHINESE || id == QJS_CAL_DANGI) {
                if (p->cyclic[width] != (UINT64_C(1) << 60) - 1) complete = 0;
            } else if ((p->eras[width] & era_bits) != era_bits) complete = 0;
        }
        if ((id == QJS_CAL_CHINESE || id == QJS_CAL_DANGI) &&
            (p->leap_named != 0x3f || !p->leap_numeric)) complete = 0;
        if (complete) mask |= UINT32_C(1) << id;
    }
    /* A partially supplied new ISO calendar must fail its own proof. */
    if (!proof[QJS_CAL_ISO8601].present && (mask & (UINT32_C(1) << QJS_CAL_GREGORY)))
        mask |= UINT32_C(1) << QJS_CAL_ISO8601;
    *out = mask; return QJS_INTL_OK;
}
