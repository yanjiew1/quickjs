/* Native provider selection, fallback, ownership and exact transition tests. */
#include "../src/timezone/timezone.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "timezone_alignment_allocator.h"

typedef struct Fixture {
    unsigned char bytes[256];
    size_t size;
    int status, reads, releases;
    char requested[256];
} Fixture;
static void put32(unsigned char *p, int32_t value)
{
    uint32_t bits = (uint32_t)value;
    p[0] = bits >> 24; p[1] = bits >> 16; p[2] = bits >> 8; p[3] = bits;
}
static void fixed_fixture(Fixture *fixture, int32_t offset)
{
    int block;
    memset(fixture, 0, sizeof *fixture);
    for (block = 0; block < 2; block++) {
        unsigned char *p = fixture->bytes + block * 54;
        memcpy(p, "TZif2", 5);
        put32(p + 36, 1); /* one local type */
        put32(p + 40, 4); /* abbreviation */
        put32(p + 44, offset);
        memcpy(p + 50, "FIX", 4);
    }
    memcpy(fixture->bytes + 108, "\n\n", 2);
    fixture->size = 110;
}
static void indicator_fixture(Fixture *fixture, unsigned char standard,
                              unsigned char universal)
{
    int block;
    memset(fixture, 0, sizeof *fixture);
    for (block = 0; block < 2; block++) {
        unsigned char *p = fixture->bytes + block * 56;
        memcpy(p, "TZif2", 5);
        put32(p + 20, 1); put32(p + 24, 1);
        put32(p + 36, 1); put32(p + 40, 4);
        memcpy(p + 50, "FIX", 4);
        p[54] = standard; p[55] = universal;
    }
    memcpy(fixture->bytes + 112, "\n\n", 2);
    fixture->size = 114;
}
static void extreme_tail_fixture(Fixture *fixture)
{
    static const char tail[] = "\nEST5EDT,M3.2.0,M11.1.0\n";
    size_t position = 0;
    int block;
    memset(fixture, 0, sizeof *fixture);
    for (block = 0; block < 2; block++) {
        unsigned char *p = fixture->bytes + position;
        size_t type_position = 44;
        memcpy(p, "TZif2", 5);
        put32(p + 32, block); /* 64-bit block starts at INT64_MIN. */
        put32(p + 36, 2); put32(p + 40, 8);
        if (block) {
            p[44] = 0x80; /* big-endian INT64_MIN; other seven bytes zero */
            p[52] = 0;
            type_position += 9;
        }
        put32(p + type_position, -18000);
        put32(p + type_position + 6, -14400);
        p[type_position + 10] = 1;
        p[type_position + 11] = 4;
        memcpy(p + type_position + 12, "EST\0EDT", 8);
        position += type_position + 20;
    }
    memcpy(fixture->bytes + position, tail, sizeof tail - 1);
    fixture->size = position + sizeof tail - 1;
}
static int fixture_read(void *opaque, const char *id, const unsigned char **bytes, size_t *size)
{
    Fixture *fixture = opaque;
    fixture->reads++;
    strcpy(fixture->requested, id);
    if (fixture->status) return fixture->status;
    *bytes = fixture->bytes; *size = fixture->size;
    return 0;
}
static void fixture_release(void *opaque, const unsigned char *bytes, size_t size)
{
    Fixture *fixture = opaque;
    assert(bytes == fixture->bytes && size == fixture->size);
    fixture->releases++;
}
static void *fail_allocate(void *opaque, size_t size)
{ (void)opaque; (void)size; return NULL; }
static void ignore_deallocate(void *opaque, void *p)
{ (void)opaque; (void)p; assert(0); }
static void expect_fallback(Fixture *fixture)
{
    QJSTzSource source = {fixture, fixture_read, fixture_release};
    QJSTimeZone *zone;
    int32_t offset;
    assert(!qjs_tz_open(&zone, "America/New_York", 16, &source, NULL));
    assert(qjs_tz_origin(zone) == QJS_TZ_EMBEDDED);
    assert(!qjs_tz_offset(zone, INT64_C(1704067200), &offset) && offset == -18000);
    qjs_tz_close(zone);
}
static void alignment_tests(void)
{
    Fixture fixture;
    TzAlignmentAllocator witness = {0};
    QJSTzAllocator allocator = {
        &witness, tz_alignment_allocate, tz_alignment_deallocate
    };
    QJSTzSource source = {&fixture, fixture_read, fixture_release};
    QJSTimeZone *zone = NULL;
    QJSTzProvider *provider = NULL;
    const QJSTimeZone *primary = NULL, *alias;
    int32_t offset;

    fixed_fixture(&fixture, 1234);
    assert(!qjs_tz_open(&zone, "America/New_York", 16,
                        &source, &allocator));
    assert(!qjs_tz_offset(zone, 0, &offset) && offset == 1234);
    assert(fixture.reads == 1 && fixture.releases == 1);
    qjs_tz_close(zone);
    zone = NULL;
    witness.fail_next = 1;
    assert(qjs_tz_open(&zone, "UTC", 3, &source, &allocator) ==
           QJS_TZ_MEMORY);
    assert(!zone && fixture.reads == 1 && !witness.live);
    fixture.status = QJS_TZ_MEMORY;
    assert(qjs_tz_open(&zone, "UTC", 3, &source, &allocator) ==
           QJS_TZ_MEMORY);
    assert(!zone && !witness.live && fixture.releases == 1);
    fixture.status = QJS_TZ_ABSENT;
    assert(!qjs_tz_open(&zone, "America/New_York", 16,
                        &source, &allocator));
    assert(qjs_tz_origin(zone) == QJS_TZ_EMBEDDED);
    assert(!qjs_tz_offset(zone, 0, &offset) && offset == -18000);
    qjs_tz_close(zone);

    witness.fail_next = 1;
    assert(qjs_tz_provider_create(&provider, &source, &allocator) ==
           QJS_TZ_MEMORY);
    assert(!provider && !witness.live);
    assert(!qjs_tz_provider_create(&provider, &source, &allocator));
    witness.fail_next = 1;
    assert(qjs_tz_provider_open(provider, "America/New_York", 16,
                                &primary) == QJS_TZ_MEMORY);
    assert(!primary && witness.live == 1);
    fixed_fixture(&fixture, 1234);
    fixture.status = QJS_TZ_MEMORY;
    assert(qjs_tz_provider_open(provider, "America/New_York", 16,
                                &primary) == QJS_TZ_MEMORY);
    assert(!primary && witness.live == 1 && !fixture.releases);
    fixture.status = QJS_TZ_OK;
    assert(!qjs_tz_provider_open(provider, "America/New_York", 16,
                                 &primary));
    assert(!qjs_tz_provider_open(provider, "US/Eastern", 10, &alias));
    assert(primary == alias && witness.live == 3);
    assert(fixture.reads == 2 && fixture.releases == 1);
    assert(!qjs_tz_offset(alias, 0, &offset) && offset == 1234);
    qjs_tz_provider_free(provider);
    assert(!witness.live && witness.allocations == witness.frees);

    /* Successful source read followed by copy allocation failure still
       releases once, returns MEMORY and leaves no cached/partial snapshot. */
    fixed_fixture(&fixture, 1234);
    zone = NULL;
    witness.fail_on_attempt = witness.attempts + 2;
    assert(qjs_tz_open(&zone, "America/New_York", 16, &source, &allocator) == QJS_TZ_MEMORY);
    assert(!zone && fixture.reads == 1 && fixture.releases == 1 && !witness.live);
    witness.fail_on_attempt = 0;
    assert(!qjs_tz_open(&zone, "America/New_York", 16, &source, &allocator));
    memset(fixture.bytes, 0xff, fixture.size); /* Source storage may now change. */
    assert(!qjs_tz_offset(zone, 0, &offset) && offset == 1234);
    qjs_tz_close(zone);
    assert(!witness.live && witness.allocations == witness.frees);
}

static void provider_tests(void)
{
    Fixture fixture;
    QJSTzSource source = {&fixture, fixture_read, fixture_release};
    QJSTzAllocator fail = {NULL, fail_allocate, ignore_deallocate};
    QJSTimeZone *zone = (void *)1;
    int32_t offset;
    fixed_fixture(&fixture, 1234);
    assert(!qjs_tz_open(&zone, "US/Eastern", 10, &source, NULL));
    assert(!strcmp(fixture.requested, "America/New_York"));
    assert(fixture.reads == 1 && fixture.releases == 1);
    assert(qjs_tz_origin(zone) == QJS_TZ_SYSTEM);
    assert(!qjs_tz_offset(zone, 0, &offset) && offset == 1234);
    qjs_tz_close(zone);
    fixed_fixture(&fixture, 1234);
    assert(!qjs_tz_open(&zone, "Etc/UTC", 7, &source, NULL));
    assert(qjs_tz_origin(zone) == QJS_TZ_EMBEDDED);
    assert(!qjs_tz_offset(zone, 0, &offset) && !offset);
    qjs_tz_close(zone);
    zone = (void *)1;
    assert(qjs_tz_open(&zone, "UTC", 3, &source, &fail) == QJS_TZ_MEMORY);
    assert(zone == (void *)1 && fixture.reads == 1);
    fixed_fixture(&fixture, 0); fixture.status = QJS_TZ_MEMORY;
    assert(qjs_tz_open(&zone, "UTC", 3, &source, NULL) == QJS_TZ_MEMORY);
    assert(zone == (void *)1 && fixture.releases == 0);
    fixed_fixture(&fixture, 0); fixture.status = QJS_TZ_ABSENT;
    expect_fallback(&fixture); assert(fixture.releases == 0);
    fixed_fixture(&fixture, 0); fixture.bytes[0] = 'X';
    expect_fallback(&fixture); assert(fixture.releases == 1);
    fixed_fixture(&fixture, 0); fixture.size--;
    expect_fallback(&fixture); assert(fixture.releases == 1);
    fixed_fixture(&fixture, 0); fixture.bytes[4] = 0;
    expect_fallback(&fixture);
    fixed_fixture(&fixture, 0); put32(fixture.bytes + 28, 1);
    expect_fallback(&fixture); /* right/ leap-aware data */
    fixed_fixture(&fixture, 0);
    memcpy(fixture.bytes + 50, "-00", 4);
    memcpy(fixture.bytes + 104, "-00", 4);
    expect_fallback(&fixture); /* a range-limited unknown provider */
    fixed_fixture(&fixture, 0); fixture.bytes[5] = 1;
    expect_fallback(&fixture); /* nonzero reserved header byte */
    indicator_fixture(&fixture, 2, 0);
    expect_fallback(&fixture); /* indicator is not Boolean */
    indicator_fixture(&fixture, 0, 1);
    expect_fallback(&fixture); /* UT indicator requires standard indicator */
    {
        int64_t transition;
        int found;
        extreme_tail_fixture(&fixture);
        assert(!qjs_tz_open(&zone, "America/New_York", 16, &source, NULL));
        assert(qjs_tz_origin(zone) == QJS_TZ_SYSTEM);
        assert(!qjs_tz_offset(zone, INT64_C(1704067200), &offset) && offset == -18000);
        assert(!qjs_tz_transition(zone, INT64_C(1704067200), 1, 0, &transition, &found));
        assert(found && transition == INT64_C(1710054000));
        /* The repeat table is close to INT64_MIN. Ordinary queries require
           unsigned distance, including next/previous cyclic reconstruction. */
        assert(!qjs_tz_transition(zone, transition, 0, 1, &transition, &found) && found);
        assert(transition == INT64_C(1710054000));
        qjs_tz_close(zone);
    }
    {
        QJSTzProvider *provider, *next_provider;
        const QJSTimeZone *first, *alias, *fresh;
        fixed_fixture(&fixture, 1234);
        assert(!qjs_tz_provider_create(&provider, &source, NULL));
        assert(!qjs_tz_provider_open(provider, "America/New_York", 16, &first));
        fixed_fixture(&fixture, 5678); /* simulated system replacement */
        assert(!qjs_tz_provider_open(provider, "US/Eastern", 10, &alias));
        assert(first == alias && fixture.reads == 0);
        assert(!qjs_tz_offset(alias, 0, &offset) && offset == 1234);
        assert(!qjs_tz_provider_create(&next_provider, &source, NULL));
        assert(!qjs_tz_provider_open(next_provider, "US/Eastern", 10, &fresh));
        assert(!qjs_tz_offset(fresh, 0, &offset) && offset == 5678);
        qjs_tz_provider_free(next_provider);
        qjs_tz_provider_free(provider);
    }
}
static void embedded_tests(void)
{
    Fixture fixture = {{0}, 0, QJS_TZ_ABSENT, 0, 0, {0}};
    QJSTzSource absent = {&fixture, fixture_read, fixture_release};
    QJSTimeZone *zone;
    const char *id, *primary;
    size_t i;
    int32_t offsets[2], offset;
    int64_t transition, previous;
    int found;
    assert(!qjs_tz_resolve("aMeRiCa/NeW_yOrK", 16, &id, &primary));
    assert(!strcmp(id, "America/New_York") && !strcmp(primary, id));
    assert(!qjs_tz_resolve("Etc/UTC", 7, &id, &primary) && !strcmp(primary, "UTC"));
    assert(!qjs_tz_resolve("Pacific/Truk", 12, &id, &primary) && !strcmp(primary, "Pacific/Chuuk"));
    assert(!qjs_tz_resolve("Atlantic/Jan_Mayen", 18, &id, &primary) && !strcmp(primary, "Arctic/Longyearbyen"));
    {
        static const char *const country_aliases[][2] = {
            {"Africa/Asmera", "Africa/Asmara"},
            {"Africa/Timbuktu", "Africa/Bamako"},
            {"America/Coral_Harbour", "America/Atikokan"},
            {"America/Virgin", "America/St_Thomas"},
            {"Antarctica/South_Pole", "Antarctica/McMurdo"},
            {"Iceland", "Atlantic/Reykjavik"},
            {"Pacific/Ponape", "Pacific/Pohnpei"},
            {"Pacific/Yap", "Pacific/Chuuk"},
            {"Europe/Bratislava", "Europe/Bratislava"},
            {"GMT", "UTC"}, {"Zulu", "UTC"}, {"Universal", "UTC"},
        };
        size_t n;
        for (n = 0; n < sizeof country_aliases / sizeof *country_aliases; n++) {
            const char *alias = country_aliases[n][0];
            assert(!qjs_tz_resolve(alias, strlen(alias), &id, &primary));
            assert(!strcmp(primary, country_aliases[n][1]));
        }
    }
    assert(qjs_tz_resolve("../../etc/passwd", 16, &id, &primary));
    assert(qjs_tz_resolve("EST5EDT,M3.2.0,M11.1.0", 21, &id, &primary));
    for (i = 0; i < qjs_tz_identifier_count(); i++) {
        const char *name = qjs_tz_identifier_at(i);
        assert(!qjs_tz_open(&zone, name, strlen(name), &absent, NULL));
        assert(qjs_tz_origin(zone) == QJS_TZ_EMBEDDED);
        assert(!qjs_tz_offset(zone, -INT64_C(8640000000000), &offset));
        assert(!qjs_tz_offset(zone, INT64_C(8640000000000), &offset));
        qjs_tz_close(zone);
    }
    assert(!qjs_tz_open(&zone, "America/New_York", 16, &absent, NULL));
    assert(!qjs_tz_local_offsets(zone, INT64_C(1710037800), offsets));
    assert(offsets[0] == -18000 && offsets[1] == -14400); /* 2024 gap */
    assert(!qjs_tz_local_offsets(zone, INT64_C(1730597400), offsets));
    assert(offsets[0] == -14400 && offsets[1] == -18000); /* 2024 overlap */
    assert(!qjs_tz_transition(zone, INT64_C(1704067200), 1, 0, &transition, &found));
    assert(found && transition == INT64_C(1710054000));
    assert(!qjs_tz_transition(zone, transition, 0, 1, &previous, &found));
    assert(found && previous == transition);
    assert(!qjs_tz_transition(zone, transition, 0, 0, &previous, &found));
    assert(found && previous < transition);
    assert(!qjs_tz_transition(zone, INT64_C(8000000000000), 1, 0, &transition, &found));
    assert(found && transition > INT64_C(8000000000000)); /* recurring tail */
    qjs_tz_close(zone);
    assert(!qjs_tz_open(&zone, "Europe/Amsterdam", 16, &absent, NULL));
    assert(!qjs_tz_offset(zone, -INT64_C(2208988800), &offset) && offset == 1172);
    qjs_tz_close(zone);
}
/* Source-derived transition fixtures use the compiled 2026e data only.
   They never select host zoneinfo and need no ICU or Temporal C library. */
static void transition_expect(const QJSTimeZone *zone, int64_t input,
                              int next, int inclusive, int64_t expected)
{
    int64_t actual;
    int32_t before, after;
    int found;
    assert(!qjs_tz_transition(zone, input, next, inclusive, &actual, &found));
    assert(found && actual == expected);
    assert(!qjs_tz_offset(zone, actual - 1, &before));
    assert(!qjs_tz_offset(zone, actual, &after) && before != after);
}
static void transition_none(const QJSTimeZone *zone, int64_t input,
                            int next, int inclusive)
{
    int64_t actual;
    int found;
    assert(!qjs_tz_transition(zone, input, next, inclusive, &actual, &found));
    assert(!found);
}
static void transition_boundary(const QJSTimeZone *zone, int64_t at,
                               int64_t previous, int64_t next, int32_t delta)
{
    int32_t before, after;
    transition_expect(zone, at - 1, 1, 0, at);
    transition_expect(zone, at + 1, 0, 0, at);
    transition_expect(zone, at, 1, 1, at);
    transition_expect(zone, at, 0, 1, at);
    transition_expect(zone, at, 0, 0, previous);
    transition_expect(zone, at, 1, 0, next);
    transition_expect(zone, at - 1, 0, 0, previous);
    transition_expect(zone, at + 1, 1, 0, next);
    assert(!qjs_tz_offset(zone, at - 1, &before));
    assert(!qjs_tz_offset(zone, at, &after));
    assert(after - before == delta);
}
static void transition_noop(const QJSTimeZone *zone, int64_t at,
                           int64_t previous, int64_t next, int32_t offset)
{
    int adjustment;
    int32_t actual;
    for (adjustment = -1; adjustment <= 1; adjustment++) {
        int64_t input = at + adjustment;
        assert(!qjs_tz_offset(zone, input, &actual) && actual == offset);
        transition_expect(zone, input, 0, 0, previous);
        transition_expect(zone, input, 1, 0, next);
        /* Inclusive lookup must also reject the no-op candidate itself. */
        transition_expect(zone, input, 0, 1, previous);
        transition_expect(zone, input, 1, 1, next);
    }
}
static void put64(unsigned char *p, int64_t value)
{
    uint64_t bits = (uint64_t)value;
    int i;
    for (i = 7; i >= 0; i--) { p[i] = (unsigned char)bits; bits >>= 8; }
}
static void transition_bounds_fixture(Fixture *fixture)
{
    static const int64_t times[] = {
        -INT64_C(8640000000001), -INT64_C(8640000000000),
         INT64_C(8640000000000),  INT64_C(8640000000001)
    };
    size_t position = 0;
    int block;
    memset(fixture, 0, sizeof *fixture);
    for (block = 0; block < 2; block++) {
        unsigned char *p = fixture->bytes + position;
        size_t types = 44;
        int i;
        memcpy(p, "TZif2", 5);
        put32(p + 32, block ? 4 : 0);
        put32(p + 36, 2); put32(p + 40, 8);
        if (block) {
            for (i = 0; i < 4; i++) {
                put64(p + 44 + i * 8, times[i]);
                p[44 + 32 + i] = (unsigned char)(!(i & 1));
            }
            types += 36;
        }
        put32(p + types, 0);
        put32(p + types + 6, 3600);
        p[types + 11] = 4;
        memcpy(p + types + 12, "ZER\0ONE", 8);
        position += types + 20;
    }
    memcpy(fixture->bytes + position, "\n\n", 2);
    fixture->size = position + 2;
}
static void extra_transition_tests(void)
{
    const int64_t limit = INT64_C(8640000000000);
    const int64_t cycle = INT64_C(12622780800);
    QJSTimeZone *zone;
    int32_t before, after;
    int64_t actual, shift;
    int found, n;
    static const char *const constant[] = {"UTC", "Etc/UTC", "Etc/GMT+5"};

    assert(!qjs_tz_open_embedded(&zone, "America/New_York", 16, NULL));
    assert(qjs_tz_origin(zone) == QJS_TZ_EMBEDDED);
    /* northamerica:346-351,182-183: first actual transition, negative epoch. */
    transition_expect(zone, -INT64_C(2717650801), 1, 0, -INT64_C(2717650800));
    transition_none(zone, -INT64_C(2717650801), 0, 0);
    transition_none(zone, -INT64_C(2717650800), 0, 0);
    transition_expect(zone, -INT64_C(2717650799), 0, 0, -INT64_C(2717650800));
    transition_expect(zone, -INT64_C(2717650800), 1, 0, -INT64_C(1633280400));
    transition_expect(zone, -INT64_C(2717650799), 1, 0, -INT64_C(1633280400));
    transition_expect(zone, -INT64_C(2717650800), 0, 1, -INT64_C(2717650800));
    assert(!qjs_tz_offset(zone, -INT64_C(2717650801), &before) && before == -17762);
    assert(!qjs_tz_offset(zone, -INT64_C(2717650800), &after) && after == -18000);
    /* War -> Peace changes the abbreviation, retaining SAVE=1:00. */
    transition_noop(zone, -INT64_C(769395600), -INT64_C(880218000),
                    -INT64_C(765396000), -14400);
    for (n = 0; n < 2; n++) {
        shift = n * cycle;
        /* Last parser-generated transition and first recurrence after it. */
        transition_boundary(zone, INT64_C(14795503200) + shift,
                            INT64_C(14774943600) + shift,
                            INT64_C(14806393200) + shift, -3600);
        transition_boundary(zone, INT64_C(14806393200) + shift,
                            INT64_C(14795503200) + shift,
                            INT64_C(14826952800) + shift, 3600);
    }
    transition_none(zone, limit, 1, 0);
    transition_none(zone, -limit, 0, 0);
    transition_expect(zone, -limit, 1, 0, -INT64_C(2717650800));
    assert(!qjs_tz_transition(zone, limit, 0, 0, &actual, &found));
    assert(found && actual < limit && actual >= -limit);
    assert(!qjs_tz_offset(zone, actual - 1, &before));
    assert(!qjs_tz_offset(zone, actual, &after) && before != after);
    qjs_tz_close(zone);

    assert(!qjs_tz_open_embedded(&zone, "Europe/Lisbon", 13, NULL));
    transition_noop(zone, INT64_C(717555600), INT64_C(701830800),
                    INT64_C(733280400), 3600);
    qjs_tz_close(zone);
    assert(!qjs_tz_open_embedded(&zone, "Australia/Lord_Howe", 19, NULL));
    for (n = 0; n < 2; n++) {
        shift = n * cycle;
        transition_boundary(zone, INT64_C(14792427000) + shift,
                            INT64_C(14776700400) + shift,
                            INT64_C(14808150000) + shift, 1800);
        transition_boundary(zone, INT64_C(14808150000) + shift,
                            INT64_C(14792427000) + shift,
                            INT64_C(14823876600) + shift, -1800);
    }
    transition_none(zone, limit, 1, 0);
    transition_none(zone, -limit, 0, 0);
    qjs_tz_close(zone);
    for (n = 0; n < (int)(sizeof constant / sizeof *constant); n++) {
        assert(!qjs_tz_open_embedded(&zone, constant[n], strlen(constant[n]), NULL));
        transition_none(zone, -limit, 0, 0);
        transition_none(zone, -limit, 1, 0);
        transition_none(zone, 0, 0, 0);
        transition_none(zone, 0, 1, 0);
        transition_none(zone, limit, 0, 0);
        transition_none(zone, limit, 1, 0);
        qjs_tz_close(zone);
    }
    {
        Fixture fixture;
        QJSTzSource source = {&fixture, fixture_read, fixture_release};
        transition_bounds_fixture(&fixture);
        assert(!qjs_tz_open(&zone, "America/New_York", 16, &source, NULL));
        assert(qjs_tz_origin(zone) == QJS_TZ_SYSTEM);
        assert(fixture.reads == 1 && fixture.releases == 1);
        /* Beyond-bound changes exist in the input TZif but must be skipped.
           Both bound instants themselves are valid transition results. */
        transition_expect(zone, -limit - 2, 1, 0, -limit);
        transition_expect(zone, limit + 2, 0, 0, limit);
        transition_expect(zone, -limit, 1, 1, -limit);
        transition_expect(zone, -limit, 0, 1, -limit);
        transition_expect(zone, limit, 1, 1, limit);
        transition_expect(zone, limit, 0, 1, limit);
        transition_expect(zone, -limit, 1, 0, limit);
        transition_expect(zone, limit, 0, 0, -limit);
        transition_none(zone, -limit, 0, 0);
        transition_none(zone, limit, 1, 0);
        qjs_tz_close(zone);
    }
}

int main(void)
{
    alignment_tests(); provider_tests(); embedded_tests();
    extra_transition_tests(); return 0;
}
