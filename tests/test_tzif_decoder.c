/* Synthetic format and calendar witnesses independent of host/tzcode/ICU. */
#include "../src/timezone/private.h"
#include <assert.h>
#include <string.h>

typedef struct Fixture { unsigned char bytes[512]; size_t size, types, indices; } Fixture;
static void put32(unsigned char *p, int32_t value)
{
    uint32_t bits = (uint32_t)value;
    p[0] = bits >> 24; p[1] = bits >> 16; p[2] = bits >> 8; p[3] = bits;
}
static void put64(unsigned char *p, int64_t value)
{
    uint64_t bits = (uint64_t)value;
    int i;
    for (i = 7; i >= 0; i--) { p[i] = (unsigned char)bits; bits >>= 8; }
}
static void fixture(Fixture *f, char version, const int64_t *times,
                    const unsigned char *indices, size_t count,
                    int32_t standard, int32_t daylight, const char *tail)
{
    size_t position = 0, b;
    memset(f, 0, sizeof *f);
    for (b = 0; b < 2; b++) {
        unsigned char *h = f->bytes + position;
        size_t n = b ? count : 0, types = 44 + n * 9, i;
        memcpy(h, "TZif", 4); h[4] = version;
        put32(h + 32, (int32_t)n); put32(h + 36, 2); put32(h + 40, 8);
        for (i = 0; i < n; i++) { put64(h + 44 + i * 8, times[i]); h[44 + n * 8 + i] = indices[i]; }
        put32(h + types, standard); put32(h + types + 6, daylight);
        h[types + 10] = 1; h[types + 11] = 4;
        memcpy(h + types + 12, "STD\0DST", 8);
        if (b) { f->types = position + types; f->indices = position + 44 + n * 8; }
        position += types + 20;
    }
    f->bytes[position++] = '\n';
    memcpy(f->bytes + position, tail, strlen(tail)); position += strlen(tail);
    f->bytes[position++] = '\n'; f->size = position;
}
static QJSTzifState parse(const Fixture *f)
{
    QJSTzifState state;
    assert(!qjs_tzif_parse(&state, f->bytes, f->size)); return state;
}
static void invalid(const Fixture *f)
{
    QJSTzifState state, before;
    memset(&state, 0x5a, sizeof state); memcpy(&before, &state, sizeof state);
    assert(qjs_tzif_parse(&state, f->bytes, f->size) == QJS_TZ_INVALID);
    assert(!memcmp(&state, &before, sizeof state));
}
static void offset(const QJSTzifState *s, int64_t at, int32_t expected)
{
    int32_t actual;
    assert(!qjs_tzif_offset(s, at, &actual) && actual == expected);
}
static void transition(const QJSTzifState *s, int64_t input, int next,
                        int inclusive, int64_t expected)
{
    int64_t actual; int found;
    assert(!qjs_tzif_transition(s, input, next, inclusive, &actual, &found));
    assert(found && actual == expected);
}
static void none(const QJSTzifState *s, int64_t input, int next, int inclusive)
{
    int64_t actual = 42; int found;
    assert(!qjs_tzif_transition(s, input, next, inclusive, &actual, &found));
    assert(!found && actual == 42);
}
static void framing(void)
{
    Fixture f;
    QJSTzifState s;
    int64_t times[] = {-10, 10}; unsigned char indices[] = {0, 1};
    char version;
    for (version = '2'; version <= '4'; version++) {
        fixture(&f, version, times, indices, 2, 0, 3600, ""); s = parse(&f);
        offset(&s, -11, 0); offset(&s, 10, 3600);
    }
    fixture(&f, '1', NULL, NULL, 0, 0, 0, ""); invalid(&f);
    fixture(&f, '2', NULL, NULL, 0, 0, 0, ""); f.size--; invalid(&f);
    fixture(&f, '2', NULL, NULL, 0, 0, 0, ""); f.bytes[5] = 1; invalid(&f);
    fixture(&f, '4', NULL, NULL, 0, 0, 0, ""); put32(f.bytes + 28, 1); invalid(&f);
    fixture(&f, '2', NULL, NULL, 0, 0, 0, ""); put32(f.bytes + 32, -1); invalid(&f);
    fixture(&f, '2', NULL, NULL, 0, 0, 0, ""); put32(f.bytes + 36, 0); invalid(&f);
    fixture(&f, '2', NULL, NULL, 0, 0, 0, ""); f.bytes[f.types + 4] = 2; invalid(&f);
    fixture(&f, '2', NULL, NULL, 0, 0, 0, ""); f.bytes[f.types + 5] = 8; invalid(&f);
    fixture(&f, '2', NULL, NULL, 0, 86400, 0, ""); invalid(&f);
    fixture(&f, '2', NULL, NULL, 0, -86400, 0, ""); invalid(&f);
    fixture(&f, '2', times, indices, 2, 0, 3600, ""); f.bytes[f.indices] = 2; invalid(&f);
    times[1] = -10;
    fixture(&f, '2', times, indices, 2, 0, 3600, ""); invalid(&f);
    times[1] = -11;
    fixture(&f, '2', times, indices, 2, 0, 3600, ""); invalid(&f);
    fixture(&f, '3', NULL, NULL, 0, 0, 0, "UTC0\nUTC0"); invalid(&f);
    fixture(&f, '3', NULL, NULL, 0, 0, 0, "UTC0"); f.bytes[f.size - 2] = 0; invalid(&f);
    fixture(&f, '3', NULL, NULL, 0, 0, 0, "STD0DST"); invalid(&f); /* Missing rules cannot use host defaults. */
    fixture(&f, '3', NULL, NULL, 0, 0, 0, "STD0DST,M0.1.0,M11.1.0"); invalid(&f);
    fixture(&f, '3', NULL, NULL, 0, 0, 0, "STD0DST,J0,J365"); invalid(&f);
    fixture(&f, '3', NULL, NULL, 0, 0, 0, "STD0DST,0/168,365"); invalid(&f);
    fixture(&f, '2', NULL, NULL, 0, 0, 0, "STD0DST,0/-1,365"); invalid(&f);
    fixture(&f, '3', NULL, NULL, 0, 0, 0, "STD24"); invalid(&f);
    fixture(&f, '3', NULL, NULL, 0, 0, 0, "STD0DST,M3.2.0/2:60,M11.1.0"); invalid(&f);
    {
        int64_t at = 0; unsigned char type = 1;
        fixture(&f, '3', &at, &type, 1, 0, 3600, "UTC0"); invalid(&f);
    }
}
static void future(void)
{
    const int64_t spring = INT64_C(1710054000), autumn = INT64_C(1730613600);
    const int64_t cycle = INT64_C(12622780800);
    Fixture f; QJSTzifState s; int32_t offsets[2]; int n;
    int64_t first = INT64_MIN; unsigned char type = 0;
    fixture(&f, '2', &first, &type, 1, -18000, -14400, "EST5EDT,M3.2.0,M11.1.0"); s = parse(&f);
    offset(&s, INT64_C(1704067200), -18000);
    transition(&s, INT64_C(1704067200), 1, 0, spring);
    transition(&s, spring, 1, 1, spring); transition(&s, spring, 0, 1, spring);
    transition(&s, spring, 1, 0, autumn); transition(&s, autumn, 0, 0, spring);
    assert(!qjs_tzif_local_offsets(&s, INT64_C(1710037800), offsets));
    assert(offsets[0] == -18000 && offsets[1] == -14400);
    assert(!qjs_tzif_local_offsets(&s, INT64_C(1730597400), offsets));
    assert(offsets[0] == -14400 && offsets[1] == -18000);
    for (n = -500; n <= 500; n += 100) {
        int64_t shift = cycle * n;
        transition(&s, spring + shift - 1, 1, 0, spring + shift);
        transition(&s, autumn + shift + 1, 0, 0, autumn + shift);
        offset(&s, spring + shift - 1, -18000); offset(&s, spring + shift, -14400);
    }
    fixture(&f, '3', NULL, NULL, 0, -18000, -14400, "EST5EDT,M3.2.0/-1,M11.1.0/26"); s = parse(&f);
    transition(&s, INT64_C(1704067200), 1, 0, INT64_C(1710043200));
    transition(&s, INT64_C(1710043200), 1, 0, INT64_C(1730700000));
    fixture(&f, '3', NULL, NULL, 0, -18000, -14400, "EST5EDT,0/0,J365/25"); s = parse(&f);
    assert(s.future == 3); offset(&s, 0, -14400); offset(&s, QJS_TZ_INSTANT_LIMIT, -14400);
    none(&s, 0, 1, 0); none(&s, 0, 0, 1);
    fixture(&f, '3', NULL, NULL, 0, 36000, 39600, "STD-10DST-11,M10.1.0,M4.1.0/3"); s = parse(&f);
    offset(&s, INT64_C(1704067200), 39600); offset(&s, INT64_C(1719792000), 36000);
    fixture(&f, '4', NULL, NULL, 0, 1172, 1172, "<+0017>-0:19:32"); s = parse(&f);
    offset(&s, -QJS_TZ_INSTANT_LIMIT, 1172); offset(&s, QJS_TZ_INSTANT_LIMIT, 1172);
    none(&s, 0, 1, 0);
    fixture(&f, '3', NULL, NULL, 0, 0, 3600, "STD0DST-1,J60/0,J61/0"); s = parse(&f);
    transition(&s, INT64_C(1704067200), 1, 0, INT64_C(1709251200));
    fixture(&f, '3', NULL, NULL, 0, 0, 3600, "STD0DST-1,59/0,60/0"); s = parse(&f);
    transition(&s, INT64_C(1704067200), 1, 0, INT64_C(1709164800));
    fixture(&f, '3', NULL, NULL, 0, 0, 0, "STD0DST0,M3.2.0,M11.1.0"); s = parse(&f);
    none(&s, 0, 1, 0); none(&s, 0, 0, 1);
    /* A legal extended tail has seasons only when a leap year begins on
       Monday: next event can be 28 years away, not merely one/four years. */
    fixture(&f, '3', NULL, NULL, 0, 0, 3600, "STD0DST-1,M1.1.0/0,365/145"); s = parse(&f);
    transition(&s, INT64_C(1736121601), 1, 0, INT64_C(2588198400));
    transition(&s, INT64_C(2051222400), 0, 0, INT64_C(1736121600));
    offset(&s, INT64_C(1704672000), 3600); offset(&s, INT64_C(1736208000), 0);
}
static void history_and_limits(void)
{
    const int64_t historical = -INT64_C(2717650800), limit = QJS_TZ_INSTANT_LIMIT;
    int64_t times[4] = {-QJS_TZ_INSTANT_LIMIT - 1, -QJS_TZ_INSTANT_LIMIT,
                        QJS_TZ_INSTANT_LIMIT, QJS_TZ_INSTANT_LIMIT + 1};
    unsigned char indices[4] = {1, 0, 1, 0}, type = 1;
    Fixture f; QJSTzifState s; int32_t actual;
    fixture(&f, '2', &historical, &type, 1, -17762, -18000, ""); s = parse(&f);
    offset(&s, historical - 1, -17762); offset(&s, historical, -18000);
    transition(&s, historical - 1, 1, 0, historical);
    none(&s, historical, 0, 0); transition(&s, historical + 1, 0, 0, historical);
    times[0] = -1; times[1] = 0; indices[0] = 0; indices[1] = 1;
    fixture(&f, '2', times, indices, 2, 0, 3600, ""); s = parse(&f);
    transition(&s, -2, 1, 0, 0); transition(&s, -1, 1, 1, 0); none(&s, -1, 0, 1);
    times[0] = -limit - 1; times[1] = -limit; indices[0] = 1; indices[1] = 0;
    fixture(&f, '2', times, indices, 4, 0, 3600, ""); s = parse(&f);
    transition(&s, -limit - 1, 1, 0, -limit); transition(&s, limit + 1, 0, 0, limit);
    none(&s, -limit, 0, 0); none(&s, limit, 1, 0);
    transition(&s, -limit, 0, 1, -limit); transition(&s, limit, 1, 1, limit);
    assert(qjs_tzif_offset(&s, QJS_TZ_OFFSET_LIMIT + 1, &actual) == QJS_TZ_RANGE);
    assert(qjs_tzif_offset(&s, -QJS_TZ_OFFSET_LIMIT - 1, &actual) == QJS_TZ_RANGE);
}
int main(void)
{ framing(); future(); history_and_limits(); return 0; }
