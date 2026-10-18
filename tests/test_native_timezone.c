/* Embedded IANA identities, generated views, and real transition witnesses.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include <assert.h>
#include <string.h>
#include "../src/timezone/timezone-native.h"

static uint32_t read32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | p[3];
}

static const QJSTimeZone *lookup(const char *name)
{
    const QJSTimeZone *zone = NULL;

    assert(!qjs_timezone_lookup(&zone, name, strlen(name)));
    assert(zone);
    return zone;
}

static void compare_rule(QJSTzifRule a, QJSTzifRule b)
{
    assert(a.kind == b.kind && a.day == b.day && a.month == b.month &&
           a.week == b.week && a.seconds == b.seconds);
}

static void compare_view(const QJSTzifState *a, const QJSTzifState *b)
{
    assert(a->times == b->times && a->indices == b->indices &&
           a->types == b->types && a->characters == b->characters);
    assert(a->time_count == b->time_count && a->type_count == b->type_count &&
           a->character_count == b->character_count && a->future == b->future &&
           a->standard_offset == b->standard_offset &&
           a->daylight_offset == b->daylight_offset);
    compare_rule(a->start, b->start);
    compare_rule(a->end, b->end);
}

static void test_embedded_records(void)
{
    const unsigned char *data = qjs_native_timezone_data;
    size_t size = qjs_native_timezone_data_size;
    uint32_t count, index, strings, payload;
    size_t i, utc_count = 0, primary_count = 0;

    assert(size >= 48 && !memcmp(data, "QJTZ", 4));
    assert(read32(data + 4) == 2 && read32(data + 8) == size);
    count = read32(data + 12);
    index = read32(data + 16);
    strings = read32(data + 20);
    payload = read32(data + 24);
    assert(index == 48 && strings == index + (size_t)count * 16);
    assert(strings < payload && payload <= size);
    assert(qjs_timezone_count() == count);
    assert(qjs_timezone_backend_count() + 1 == count);
    assert(read32(data + 28) == strings);
    assert(!strcmp(qjs_timezone_data_version(), (const char *)data + strings));

    for (i = 0; i < count; i++) {
        const unsigned char *record = data + index + i * 16;
        uint32_t name_at = read32(record), primary_at = read32(record + 4);
        uint32_t offset = read32(record + 8), length = read32(record + 12);
        const char *name, *primary;
        const QJSTimeZone *zone = qjs_timezone_at(i), *canonical;
        QJSTzifState parsed;
        char folded[256];
        size_t j;

        assert(name_at >= strings && name_at < payload);
        assert(primary_at >= strings && primary_at < payload);
        name = (const char *)data + name_at;
        primary = (const char *)data + primary_at;
        assert(memchr(name, 0, payload - name_at));
        assert(memchr(primary, 0, payload - primary_at));
        assert(zone == lookup(name));
        assert(!strcmp(qjs_timezone_identifier(zone), name));
        assert(!strcmp(qjs_timezone_primary_identifier(zone), primary));
        canonical = lookup(primary);
        assert(!strcmp(qjs_timezone_identifier(canonical), primary));
        assert(!strcmp(qjs_timezone_primary_identifier(canonical), primary));
        assert(canonical->rules == zone->rules);
        assert(qjs_timezone_is_primary(zone) == !strcmp(name, primary));
        primary_count += qjs_timezone_is_primary(zone);
        if (i)
            assert(strcmp(qjs_timezone_identifier(qjs_timezone_at(i - 1)), name) < 0);
        utc_count += !strcmp(name, "UTC");
        assert(strlen(name) < sizeof(folded));
        for (j = 0; name[j]; j++)
            folded[j] = name[j] >= 'A' && name[j] <= 'Z' ?
                name[j] + ('a' - 'A') : name[j];
        folded[j] = 0;
        assert(lookup(folded) == zone);
        for (j = 0; name[j]; j++)
            folded[j] = name[j] >= 'a' && name[j] <= 'z' ?
                name[j] - ('a' - 'A') : name[j];
        assert(lookup(folded) == zone);

        assert(offset >= payload && offset <= size && length <= size - offset);
        assert(!qjs_tzif_parse(&parsed, data + offset, length));
        assert(parsed.future);
        if (strcmp(primary, "UTC")) {
            assert(zone->rules);
            compare_view(&parsed, &zone->rules->tzif);
        } else {
            int32_t value = 1;
            assert(!zone->rules);
            assert(!qjs_tzif_offset(&parsed, -QJS_TZ_INSTANT_LIMIT, &value) && !value);
            assert(!qjs_tzif_offset(&parsed, QJS_TZ_INSTANT_LIMIT, &value) && !value);
        }
    }
    assert(utc_count == 1 && primary_count > 1);
    assert(!qjs_timezone_at(count) && !qjs_timezone_backend_at(count - 1));
    for (i = 0; i < qjs_timezone_backend_count(); i++)
        assert(strcmp(qjs_timezone_identifier(qjs_timezone_backend_at(i)), "UTC"));
    assert(lookup("Africa/Abidjan")->rules); /* A political zero-offset zone. */
}

typedef struct Candidates {
    int64_t values[4];
    size_t length;
} Candidates;

static void store_epoch(void *opaque, int64_t epoch)
{
    Candidates *result = opaque;

    assert(result->length < 4);
    result->values[result->length++] = epoch;
}

static void check_offset(const QJSTimeZone *zone, int64_t epoch, int32_t expected)
{
    int32_t offset = 123;

    assert(!qjs_timezone_offset(zone, epoch, &offset));
    assert(offset == expected);
}

static void test_real_rules(void)
{
    const QJSTimeZone *ny = lookup("America/New_York");
    const QJSTimeZone *dublin = lookup("Europe/Dublin");
    const QJSTimeZone *apia = lookup("Pacific/Apia");
    const QJSTimeZone *fixed = lookup("Etc/GMT+5");
    const int64_t spring = 1710054000, autumn = 1730613600;
    const int cycles[] = {1, 100, 500};
    Candidates candidates = {{0}, 0};
    int64_t transition = 123, gap_end = 456;
    int32_t before = 123, after = 456;
    int found;
    size_t count, i;

    check_offset(ny, 1704067200, -18000);
    check_offset(ny, 1719792000, -14400);
    check_offset(ny, INT64_C(-2717650801), -17762);
    check_offset(ny, INT64_C(-2717650800), -18000);
    assert(!qjs_timezone_transition(ny, spring, 1, 1, &transition, &found));
    assert(found && transition == spring);
    assert(!qjs_timezone_transition(ny, spring, 1, 0, &transition, &found));
    assert(found && transition == autumn);
    assert(!qjs_timezone_transition(ny, autumn, 0, 0, &transition, &found));
    assert(found && transition == spring);
    assert(!qjs_timezone_candidates(ny, 1710037800, store_epoch, &candidates,
                                    &count, &before, &after, &gap_end));
    assert(!count && !candidates.length && before == -18000 && after == -14400 &&
           gap_end == spring);
    assert(!qjs_timezone_candidates(ny, 1730597400, store_epoch, &candidates,
                                    &count, &before, &after, &gap_end));
    assert(count == 2 && candidates.length == 2);
    assert(candidates.values[0] == INT64_C(1730611800) &&
           candidates.values[1] == INT64_C(1730615400));
    candidates.length = 0;
    assert(!qjs_timezone_candidates(apia, 1325246400, store_epoch, &candidates,
                                    &count, &before, &after, &gap_end));
    assert(!count && before == -36000 && after == 50400 && gap_end == 1325239200);
    check_offset(dublin, 1704067200, 0);
    check_offset(dublin, 1719792000, 3600);
    for (i = 0; i < sizeof(cycles) / sizeof(cycles[0]); i++) {
        int64_t shift = INT64_C(12622780800) * cycles[i];

        check_offset(ny, 1704067200 + shift, -18000);
        check_offset(ny, 1719792000 + shift, -14400);
        check_offset(dublin, 1704067200 + shift, 0);
        check_offset(dublin, 1719792000 + shift, 3600);
        assert(!qjs_timezone_transition(ny, spring + shift, 1, 1,
                                        &transition, &found));
        assert(found && transition == spring + shift);
    }
    check_offset(fixed, -QJS_TZ_INSTANT_LIMIT, -18000);
    check_offset(fixed, QJS_TZ_INSTANT_LIMIT, -18000);
    transition = 123;
    assert(!qjs_timezone_transition(fixed, 0, 1, 0, &transition, &found));
    assert(!found && transition == 123);
    before = 123;
    assert(qjs_timezone_offset(ny, QJS_TZ_OFFSET_LIMIT + 1, &before) == QJS_TZ_RANGE);
    assert(before == 123);
    fixed = ny;
    assert(qjs_timezone_lookup(&fixed, "America/New_York\0", 17) == QJS_TZ_RANGE);
    assert(fixed == ny);
    assert(qjs_timezone_lookup(&fixed, "America/Not_A_Zone", 18) == QJS_TZ_RANGE);
    assert(fixed == ny);
}

static void test_offset_information(void)
{
    const QJSTimeZone *ny = lookup("America/New_York");
    const QJSTimeZone *dublin = lookup("Europe/Dublin");
    const QJSTimeZone *utc = lookup("UTC");
    QJSTimeZoneInfo info;
    const int64_t epochs[] = {1704067200, 1719792000,
                             INT64_C(1704067200) + 12622780800,
                             INT64_C(1719792000) + 12622780800};
    size_t i;

    for (i = 0; i < sizeof(epochs) / sizeof(epochs[0]); i++) {
        int summer = i % 2;

        assert(!qjs_timezone_info(ny, epochs[i], &info));
        assert(info.offset_seconds == (summer ? -14400 : -18000));
        assert(info.is_dst == summer);
        /* Ireland designates winter's negative adjustment as DST. */
        assert(!qjs_timezone_info(dublin, epochs[i], &info));
        assert(info.offset_seconds == (summer ? 3600 : 0));
        assert(info.is_dst == !summer);
        assert(!qjs_timezone_info(utc, epochs[i], &info));
        assert(!info.offset_seconds && !info.is_dst);
        assert(!qjs_timezone_info(lookup("Zulu"), epochs[i], &info));
        assert(!info.offset_seconds && !info.is_dst);
    }
    info.offset_seconds = 123;
    info.is_dst = 456;
    assert(qjs_timezone_info(ny, QJS_TZ_OFFSET_LIMIT + 1, &info) == QJS_TZ_RANGE);
    assert(info.offset_seconds == 123 && info.is_dst == 456);
}

int main(void)
{
    test_embedded_records();
    test_real_rules();
    test_offset_information();
    return 0;
}
