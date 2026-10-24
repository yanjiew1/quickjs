/* Native zone names, source identity, territory and conservative fallback.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/zone-name.h"
#ifdef CONFIG_TIMEZONE_NATIVE
#include "../src/timezone/timezone-native.h"
#endif
#include "../src/cutils/cutils.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        state->live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text) { return (QJSIntlBytes){text, strlen(text)}; }

static void expected(const QJSIntlFormatted *result, const char *text)
{
    const uint8_t *at = (const uint8_t *)text, *end = at + strlen(text), *next;
    size_t position = 0, i, part_end = 0;
    while (at < end) {
        int c = unicode_from_utf8(at, (int)(end - at), &next);
        assert(c >= 0);
        if (c > 0xffff) {
            c -= 0x10000;
            assert(position + 1 < result->length);
            assert(result->text[position++] == 0xd800 + (c >> 10));
            assert(result->text[position++] == 0xdc00 + (c & 1023));
        } else {
            assert(position < result->length && result->text[position++] == c);
        }
        at = next;
    }
    assert(position == result->length);
    for (i = 0; i < result->part_count; i++) {
        assert(result->parts[i].start == part_end);
        part_end = result->parts[i].end;
        assert(part_end > result->parts[i].start && part_end <= result->length);
        assert(result->parts[i].type == QJS_INTL_PART_LITERAL ||
               ((unsigned int)result->parts[i].type >= QJS_INTL_PART_NUMBER_COUNT &&
                (unsigned int)result->parts[i].type < QJS_INTL_PART_COUNT));
    }
    assert(part_end == result->length);
}

static QJSIntlDateNumberFormats open_numbers(const QJSIntlNumberService *service, size_t locale,
                                              QJSIntlBytes system)
{
    QJSIntlDateNumberFormats result = {0};
    QJSIntlNumberOptions options = {0};
    QJSIntlNumberFormatter *formatter;
    unsigned int i;
    options.digits = (QJSIntlDecimalOptions){1, 0, 0, 1, 21, 1, QJS_INTL_ROUND_HALF_EXPAND,
                                              QJS_INTL_ROUND_FRACTION, QJS_INTL_TRAILING_ZERO_AUTO, 1000};
    options.maximum_output_length = 1000;
    assert(!qjs_intl_number_formatter_open(service, locale, system, &options, &formatter));
    result.numeric = formatter;
    options.digits.minimum_integer_digits = 2;
    assert(!qjs_intl_number_formatter_open(service, locale, system, &options, &formatter));
    result.two_digit = formatter;
    for (i = 0; i < 3; i++) {
        options.digits.minimum_integer_digits = i + 1;
        assert(!qjs_intl_number_formatter_open(service, locale, system, &options, &formatter));
        result.fraction[i] = formatter;
    }
    return result;
}

static void close_numbers(QJSIntlDateNumberFormats *numbers)
{
    unsigned int i;
    qjs_intl_number_formatter_close((QJSIntlNumberFormatter *)numbers->numeric);
    qjs_intl_number_formatter_close((QJSIntlNumberFormatter *)numbers->two_digit);
    for (i = 0; i < 3; i++)
        qjs_intl_number_formatter_close((QJSIntlNumberFormatter *)numbers->fraction[i]);
    memset(numbers, 0, sizeof(*numbers));
}

extern const uint8_t qjs_intl_date_zone_fixture_bytes[];
extern const size_t qjs_intl_date_zone_fixture_length;
extern const uint8_t qjs_intl_date_zone_gmt_bytes[];
extern const size_t qjs_intl_date_zone_gmt_length;
extern const uint8_t qjs_intl_date_zone_conflict_bytes[], qjs_intl_date_zone_gap_bytes[],
                     qjs_intl_date_zone_ambiguous_bytes[], qjs_intl_date_zone_perpetual_bytes[];
extern const size_t qjs_intl_date_zone_conflict_length, qjs_intl_date_zone_gap_length,
                    qjs_intl_date_zone_ambiguous_length, qjs_intl_date_zone_perpetual_length;


static uint32_t locale(const QJSIntlData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes value;
        assert(!qjs_intl_data_locale(data, i, &value, &parent));
        if (value.length == strlen(name) && !memcmp(value.data, name, value.length))
            return i;
    }
    abort();
}

static const QJSTimeZone *zone(const char *name)
{
    const QJSTimeZone *value;
    assert(!qjs_timezone_lookup(&value, name, strlen(name)));
    return value;
}

static void identity(const QJSIntlLocaleMetadata *metadata)
{
    static const struct { const char *input, *canonical, *region; } cases[] = {
        {"America/New_York", "America/New_York", "US"},
        {"US/Eastern", "America/New_York", "US"},
        {"Asia/Kolkata", "Asia/Calcutta", "IN"},
        {"America/Curacao", "America/Curacao", "CW"},
        {"Europe/Mariehamn", "Europe/Mariehamn", "AX"},
        {"Asia/Hebron", "Asia/Hebron", "PS"},
        {"UTC", "Etc/UTC", ""}, {"Etc/GMT+1", "Etc/GMT+1", ""},
        {"CST6CDT", "CST6CDT", ""},
    };
    QJSIntlTimeZoneIdentity result, copy, zero = {0};
    uint32_t count;
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert(!qjs_intl_timezone_identity(metadata, bytes(cases[i].input), &result));
        assert(result.identifier.length == strlen(cases[i].canonical) &&
               !memcmp(result.identifier.data, cases[i].canonical, result.identifier.length));
        assert(!strcmp(result.territory, cases[i].region));
    }
    assert(!qjs_intl_timezone_identity(metadata, bytes("America/Curacao"), &result));
    copy = result;
    assert(!qjs_intl_timezone_identity(metadata, bytes("UTC"), &result));
    assert(!strcmp(copy.territory, "CW") && !result.territory[0]);
    assert(qjs_intl_timezone_identity(metadata, bytes("Unknown/Test"), &result) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(!qjs_intl_timezone_region_count(metadata, bytes("US"), &count) && count > 1);
    assert(!qjs_intl_timezone_region_count(metadata, bytes("SG"), &count) && count == 1);
    assert(!qjs_intl_timezone_region_count(metadata, bytes("ZZ"), &count) && !count);
    assert(!qjs_intl_timezone_region_count(metadata, bytes("001"), &count) && !count);
    assert(qjs_intl_timezone_region_count(metadata, bytes("us"), &count) == QJS_INTL_INVALID_ARGUMENT && !count);
    {
        uint32_t cursor = 0, seen = 0;
        QJSIntlStatus status;
        assert(!qjs_intl_timezone_region_count(metadata, bytes("US"), &count));
        while (!(status = qjs_intl_timezone_region_next(metadata, bytes("US"), &cursor, &result))) {
            assert(!strcmp(result.territory, "US") && result.identifier.length);
            seen++;
        }
        assert(status == QJS_INTL_UNSUPPORTED && seen == count);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
}

static QJSIntlFormatted render(const QJSIntlAllocator *allocator, const QJSIntlDateZoneData *data,
                                const QJSIntlDateOffset *offset, const QJSIntlDateNumberFormats *numbers,
                                const char *name, int64_t instant, QJSIntlDateTimeZoneStyle style,
                                const char *value)
{
    QJSIntlFormatted result;
    assert(!qjs_intl_date_zone_format(allocator, data, offset, numbers, name ? zone(name) : NULL, 0, instant, style, 1000, &result));
    expected(&result, value);
    assert(result.part_count == 1 && result.parts[0].type == QJS_INTL_PART_TIME_ZONE_NAME);
    return result;
}

static void check(const QJSIntlAllocator *allocator, const QJSIntlDateZoneData *data,
                   const QJSIntlDateOffset *offset, const QJSIntlDateNumberFormats *numbers,
                   const char *name, int64_t instant, QJSIntlDateTimeZoneStyle style, const char *value)
{
    QJSIntlFormatted result = render(allocator, data, offset, numbers, name, instant, style, value);
    qjs_intl_formatted_clear(allocator, &result);
}

#ifdef CONFIG_TIMEZONE_NATIVE
/* These witnesses use actual named rules and an authored TZif descriptor. */
static void authored(const QJSIntlAllocator *allocator, const QJSIntlLocaleMetadata *metadata,
                      const QJSIntlDateNumberFormats *numbers)
{
    QJSIntlData pack;
    QJSIntlTimeZoneNameData names;
    QJSIntlTimeZonePresentationData presentation;
    QJSIntlDateZoneData data = {0};
    QJSIntlDateOffset offset;
    QJSIntlFormatted result, zero = {0};
    Allocation *state = allocator->opaque;
    size_t baseline, calls, i, limit;
    assert(!qjs_intl_data_open(qjs_intl_date_zone_fixture_bytes, qjs_intl_date_zone_fixture_length, allocator, &pack));
    assert(!qjs_intl_timezone_name_data_open(&pack, allocator, &names));
    assert(!qjs_intl_timezone_presentation_data_open(&pack, allocator, &presentation));
    data.names = &names;
    data.names_locale = locale(&pack, "en");
    data.metadata = metadata;
    data.presentation = &presentation;
    data.territory = bytes("US");
    assert(!qjs_intl_date_offset_from_data(&names, data.names_locale, allocator, &offset));
    check(allocator, &data, &offset, numbers, "Asia/Tokyo", 1719835200, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "Direct zone");
    check(allocator, &data, &offset, numbers, "America/Vancouver", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "CA — Pacific");
    data.territory = bytes("CA");
    check(allocator, &data, &offset, numbers, "America/Vancouver", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "Pacific");
    data.territory = bytes("US");
    check(allocator, &data, &offset, numbers, "America/Phoenix", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "Mountain standard");
    check(allocator, &data, &offset, numbers, "America/New_York", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "'New York'");
    check(allocator, &data, &offset, numbers, "Europe/London", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "'GB'");
    check(allocator, &data, &offset, numbers, "America/Curacao", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "'CW'");
    check(allocator, &data, &offset, numbers, "Europe/Mariehamn", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "'AX'");
    check(allocator, &data, &offset, numbers, "Asia/Shanghai", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "'CN'");
    check(allocator, &data, &offset, numbers, "CST6CDT", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "'Unknown place'");
    check(allocator, &data, &offset, numbers, "Etc/GMT+1", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "GMT-01:00");
    assert(!qjs_intl_date_zone_format(allocator, NULL, &offset, numbers, NULL, 90,
                                      1719835200, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, 1000, &result));
    expected(&result, "GMT+01:30");
    qjs_intl_formatted_clear(allocator, &result);
    check(allocator, &data, &offset, numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_SHORT, "GMT+0");
    check(allocator, &data, &offset, numbers, "Europe/Dublin", 1704110400,
          QJS_INTL_DATETIME_ZONE_LONG, "Winter label");
    check(allocator, &data, &offset, numbers, "Europe/Dublin", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG, "Summer label");
    check(allocator, &data, &offset, numbers, "America/New_York", 1719835200,
          QJS_INTL_DATETIME_ZONE_SHORT, "GMT-4");
    {
        const uint8_t *variants[] = {qjs_intl_date_zone_conflict_bytes, qjs_intl_date_zone_gap_bytes,
                                    qjs_intl_date_zone_ambiguous_bytes};
        const size_t lengths[] = {qjs_intl_date_zone_conflict_length, qjs_intl_date_zone_gap_length,
                                 qjs_intl_date_zone_ambiguous_length};
        QJSIntlTimeZonePresentationData changed;
        QJSIntlData variant;
        for (i = 0; i < 3; i++) {
            assert(!qjs_intl_data_open(variants[i], lengths[i], allocator, &variant));
            assert(!qjs_intl_timezone_presentation_data_open(&variant, allocator, &changed));
            data.presentation = &changed;
            check(allocator, &data, &offset, numbers, "America/Phoenix", 1719835200,
                  QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "'Phoenix'");
        }
        check(allocator, &data, &offset, numbers, "America/Phoenix", 1719835200,
              QJS_INTL_DATETIME_ZONE_LONG, "GMT-07:00");
        data.presentation = &presentation;
    }
    {
        static QJSTimeZoneRules rules;
        static const QJSTimeZone permanent = {"America/Phoenix", "America/Phoenix", &rules};
        assert(!qjs_tzif_parse(&rules.tzif, qjs_intl_date_zone_perpetual_bytes, qjs_intl_date_zone_perpetual_length));
        assert(rules.tzif.future == 3);
        assert(!qjs_intl_date_zone_format(allocator, &data, &offset, numbers, &permanent, 0,
                                          1719835200, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, 1000, &result));
        expected(&result, "'Phoenix'"); /* Constant raw DST is not a saving-amount proof. */
        qjs_intl_formatted_clear(allocator, &result);
    }
    assert(qjs_intl_date_zone_format(allocator, &data, &offset, numbers, zone("UTC"), 0,
                                     1719835200, (QJSIntlDateTimeZoneStyle)INT32_MIN, 1000, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    data.territory = bytes("us");
    assert(qjs_intl_date_zone_format(allocator, &data, &offset, numbers, zone("UTC"), 0,
                                     1719835200, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, 1000, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    data.territory = bytes("US");
    baseline = state->live;
    state->calls = 0;
    result = render(allocator, &data, &offset, numbers, "America/Vancouver", 1719835200,
                    QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "CA — Pacific");
    calls = state->calls;
    limit = result.length;
    qjs_intl_formatted_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        assert(qjs_intl_date_zone_format(allocator, &data, &offset, numbers, zone("America/Vancouver"), 0,
                                         1719835200, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, 1000, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    }
    state->calls = state->fail_at = 0;
    assert(qjs_intl_date_zone_format(allocator, &data, &offset, numbers, zone("America/Vancouver"), 0,
                                     1719835200, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, limit - 1, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    assert(!qjs_intl_date_zone_format(allocator, &data, &offset, numbers, zone("America/Vancouver"), 0,
                                      1719835200, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, limit, &result));
    qjs_intl_date_offset_clear(&offset);
    memset(&names, 0, sizeof(names));
    memset(&presentation, 0, sizeof(presentation));
    expected(&result, "CA — Pacific");
    qjs_intl_formatted_clear(allocator, &result);
}
#endif

static void fixed_zero_sources(const QJSIntlAllocator *allocator, const QJSIntlDateNumberFormats *numbers)
{
    QJSIntlData pack;
    QJSIntlTimeZoneNameData names;
    QJSIntlDateZoneData data = {0};
    QJSIntlDateOffset offset;

    assert(!qjs_intl_data_open(qjs_intl_date_zone_gmt_bytes, qjs_intl_date_zone_gmt_length, allocator, &pack));
    assert(!qjs_intl_timezone_name_data_open(&pack, allocator, &names));
    data.names = &names;
    data.names_locale = locale(&pack, "root");
    assert(!qjs_intl_date_offset_from_data(&names, data.names_locale, allocator, &offset));
    check(allocator, &data, &offset, numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_LONG, "Zone standard");
    check(allocator, &data, &offset, numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "Zone generic");
    /* Blocked/empty child zone leaves must not revive their root zone labels.
       An independent metazone fallback still uses its requested name type. */
    data.names_locale = locale(&pack, "en");
    check(allocator, &data, &offset, numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_LONG, "Meta standard");
    check(allocator, &data, &offset, numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "Meta generic");
    check(allocator, &data, &offset, numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_LONG_OFFSET, "GMT+00:00");
    qjs_intl_date_offset_clear(&offset);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData pack;
    QJSIntlLocaleMetadata metadata;
    QJSIntlTimeZoneNameData names;
    QJSIntlTimeZonePresentationData presentation;
    QJSIntlNameData regions;
    QJSIntlDateZoneData data = {0};
    QJSIntlNumberService *service;
    QJSIntlDateNumberFormats numbers;
    const QJSIntlBytes *tags;
    QJSIntlDateOffset offset;
    size_t en, count;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &pack));
    assert(!qjs_intl_locale_metadata_open(&pack, &allocator, &metadata));
    {
        size_t calls = state.calls;
        identity(&metadata);
        assert(state.calls == calls);
    }
    assert(!qjs_intl_number_open(&allocator, NULL, &service));
    assert(!qjs_intl_number_available(service, &tags, &count));
    for (en = 0; en < count; en++)
        if (tags[en].length == 2 && !memcmp(tags[en].data, "en", 2))
            break;
    assert(en < count);
    numbers = open_numbers(service, en, bytes("latn"));
#ifdef CONFIG_TIMEZONE_NATIVE
    authored(&allocator, &metadata, &numbers);
#endif
    fixed_zero_sources(&allocator, &numbers);
    assert(!qjs_intl_timezone_name_data_open(&pack, &allocator, &names));
    assert(!qjs_intl_timezone_presentation_data_open(&pack, &allocator, &presentation));
    assert(!qjs_intl_name_data_open(&pack, &allocator, &regions));
    data.names = &names;
    data.names_locale = locale(&pack, "en");
    data.presentation = &presentation;
    data.metadata = &metadata;
    data.regions = &regions;
    data.regions_locale = locale(&pack, "en");
    data.territory = bytes("US");
    assert(!qjs_intl_date_offset_from_data(&names, data.names_locale, &allocator, &offset));
    /* Zero custom offsets may use GMT names without becoming the UTC ID.
       Explicit offset styles, absent name data and nonzero offsets stay numeric. */
    check(&allocator, &data, &offset, &numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_SHORT, "GMT");
    check(&allocator, &data, &offset, &numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_LONG, "Greenwich Mean Time");
    check(&allocator, &data, &offset, &numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_SHORT_GENERIC, "GMT");
    check(&allocator, &data, &offset, &numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "Greenwich Mean Time");
    check(&allocator, &data, &offset, &numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_SHORT_OFFSET, "GMT+0");
    check(&allocator, &data, &offset, &numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_LONG_OFFSET, "GMT+00:00");
    check(&allocator, NULL, &offset, &numbers, NULL, 0, QJS_INTL_DATETIME_ZONE_SHORT, "GMT+0");
    check(&allocator, &data, &offset, &numbers, "UTC", 1704110400, QJS_INTL_DATETIME_ZONE_SHORT, "UTC");
    check(&allocator, &data, &offset, &numbers, "UTC", 1704110400, QJS_INTL_DATETIME_ZONE_LONG, "Coordinated Universal Time");
#ifdef CONFIG_TIMEZONE_NATIVE
    check(&allocator, &data, &offset, &numbers, "America/New_York", 1704110400, QJS_INTL_DATETIME_ZONE_SHORT, "EST");
    check(&allocator, &data, &offset, &numbers, "America/New_York", 1719835200, QJS_INTL_DATETIME_ZONE_LONG, "Eastern Daylight Time");
    check(&allocator, &data, &offset, &numbers, "America/New_York", 1719835200, QJS_INTL_DATETIME_ZONE_SHORT_GENERIC, "ET");
    check(&allocator, &data, &offset, &numbers, "America/New_York", 1719835200, QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "Eastern Time");
    check(&allocator, &data, &offset, &numbers, "America/New_York", 1719835200, QJS_INTL_DATETIME_ZONE_SHORT_OFFSET, "GMT-4");
    check(&allocator, &data, &offset, &numbers, "America/New_York", 1719835200, QJS_INTL_DATETIME_ZONE_LONG_OFFSET, "GMT-04:00");
    check(&allocator, &data, &offset, &numbers, "America/Vancouver", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG_GENERIC, "Pacific Time (Canada)");
    check(&allocator, &data, &offset, &numbers, "Africa/Casablanca", 1719835200,
          QJS_INTL_DATETIME_ZONE_LONG, "GMT+01:00");
#endif
    qjs_intl_date_offset_clear(&offset);
    close_numbers(&numbers);
    qjs_intl_number_close(service);
    assert(!state.live);
    return 0;
}
