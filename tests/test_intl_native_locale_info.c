/* Authored C unit proposal; not executed in the source packet.
 * Usage: test-intl-native-locale-info actual-intl-data.bin
 * Accepts root-generated1.1/1.2 or new1.3. Script cases require actual24.
 * Synthetic25/26/20 below prove engine selection only, never real coverage.
 * Oracles: ECMA4027ae78cf (2026-10-09), CLDR49 supplementalData.xml and
 * scriptMetadata.txt. Does not compare against ICU or generated outputs. */
#include "intl/native-locale-info.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AllocationState { size_t calls, fail_at, live; } AllocationState;
static void *test_malloc(void *opaque, size_t n)
{
    AllocationState *s = opaque;
    void *p;
    assert(n);
    if (++s->calls == s->fail_at) return NULL;
    p = malloc(n); if (p) s->live++; return p;
}
static void *test_realloc(void *opaque, void *old, size_t n)
{
    AllocationState *s = opaque;
    int was_null = old == NULL;
    void *p;
    assert(n);
    if (++s->calls == s->fail_at) return NULL;
    p = realloc(old, n); if (p && was_null) s->live++; return p;
}
static void test_free(void *opaque, void *p)
{
    AllocationState *s = opaque;
    if (p) { assert(s->live); s->live--; free(p); }
}
static QJSIntlBytes b(const char *text)
{
    QJSIntlBytes value = {text, strlen(text)}; return value;
}
static QJSIntlLocaleInfoRequest request(const char *locale)
{
    QJSIntlLocaleInfoRequest r; memset(&r, 0, sizeof(r)); r.locale = b(locale); return r;
}
static void zero_result(const QJSIntlNativeLocaleInfoResult *r)
{
    assert(!r->list.items && !r->list.count && !r->value && !r->defined &&
           !r->first_day && !r->weekend_mask && r->direction == QJS_INTL_LOCALE_DIRECTION_UNDEFINED);
}
static void check_list(const QJSIntlNativeLocaleInfoResult *r, const char *const *expected, size_t count)
{
    size_t i;
    assert(r->defined && r->list.count == count && !r->value);
    for (i = 0; i < count; i++) {
        assert(r->list.items[i].length == strlen(expected[i]));
        assert(!memcmp(r->list.items[i].data, expected[i], strlen(expected[i]) + 1));
    }
}
static void list_case(QJSIntlAllocator *a, QJSIntlDataView *view,
                      QJSIntlLocaleInfoCapabilities *cap, const char *locale,
                      QJSIntlLocaleInfoField field, const char *const *expected, size_t count)
{
    QJSIntlNativeLocaleInfoResult out;
    QJSIntlLocaleInfoRequest r = request(locale);
    assert(qjs_intl_native_locale_info_get(a, view, cap, &r, field, &out) == QJS_INTL_OK);
    check_list(&out, expected, count);
    qjs_intl_native_locale_info_result_clear(a, &out);
    zero_result(&out); assert(!((AllocationState *)a->opaque)->live);
}
static void week_case(QJSIntlAllocator *a, QJSIntlDataView *view,
                      QJSIntlLocaleInfoCapabilities *cap, const char *locale,
                      const char *fw, unsigned int first, unsigned int mask)
{
    QJSIntlNativeLocaleInfoResult out;
    QJSIntlLocaleInfoRequest r = request(locale);
    if (fw) r.first_day = b(fw);
    assert(qjs_intl_native_locale_info_get(a, view, cap, &r, QJS_INTL_LOCALE_WEEK_INFO, &out) == QJS_INTL_OK);
    assert(out.defined && out.first_day == first && out.weekend_mask == mask);
    assert(!out.list.count && !out.value);
    qjs_intl_native_locale_info_result_clear(a, &out);
    assert(!((AllocationState *)a->opaque)->live);
}

/* Minimal fixture assembler copies actual schema sections byte-for-byte and
 * adds explicitly synthetic installed capabilities. All endian operations
 * are local to this test TU; production reader helpers remain private. */
typedef struct Table { uint32_t id, width, count; unsigned char *data; } Table;
typedef struct Fixture { Table tables[32]; size_t count; } Fixture;
typedef struct Ref { uint32_t offset, length; } Ref;
static uint32_t u32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put(unsigned char *p, uint32_t n)
{
    p[0] = (unsigned char)n; p[1] = (unsigned char)(n >> 8);
    p[2] = (unsigned char)(n >> 16); p[3] = (unsigned char)(n >> 24);
}
static Table *table(Fixture *f, uint32_t id)
{
    size_t i;
    for (i = 0; i < f->count; i++) if (f->tables[i].id == id) return &f->tables[i];
    return NULL;
}
static Table *replace_table(Fixture *f, uint32_t id, uint32_t width, uint32_t count)
{
    Table *t = table(f, id);
    if (!t) { assert(f->count < 32); t = &f->tables[f->count++]; memset(t, 0, sizeof(*t)); }
    free(t->data); t->id = id; t->width = width; t->count = count;
    t->data = calloc(count ? count : 1, width); assert(t->data); return t;
}
static Ref pool_add(Fixture *f, const char *s)
{
    Table *pool = table(f, QJS_INTL_DATA_UTF8_POOL);
    Ref r;
    size_t length = strlen(s);
    assert(pool && pool->width == 1 && length < UINT32_MAX - pool->count);
    r.offset = pool->count; r.length = (uint32_t)length;
    pool->data = realloc(pool->data, (size_t)pool->count + length + 1); assert(pool->data);
    memcpy(pool->data + pool->count, s, length + 1);
    pool->count += (uint32_t)length + 1;
    return r;
}
static void ref_put(unsigned char *p, Ref r) { put(p, r.offset); put(p + 4, r.length); }
static uint32_t list_add(Fixture *f, const char *s)
{
    Table *list = table(f, QJS_INTL_DATA_LIST);
    Ref r = pool_add(f, s);
    uint32_t index;
    assert(list && list->width == 8 && list->count < UINT32_MAX / 8);
    index = list->count++;
    list->data = realloc(list->data, (size_t)list->count * 8); assert(list->data);
    ref_put(list->data + (size_t)index * 8, r); return index;
}
static void load_fixture(Fixture *f, const QJSIntlDataView *view)
{
    static const uint32_t ids[] = {1, 2, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 40, 41};
    size_t i;
    memset(f, 0, sizeof(*f));
    for (i = 0; i < sizeof(ids) / sizeof(*ids); i++) {
        QJSIntlDataSection s;
        if (qjs_intl_data_section(view, ids[i], &s) == QJS_INTL_DATA_OK) {
            Table *t = replace_table(f, ids[i], s.record_width, s.record_count);
            if (s.bytes.length) memcpy(t->data, s.bytes.data, s.bytes.length);
        }
    }
}
static uint32_t locale_index(Fixture *f, const char *tag)
{
    Table *locales = table(f, QJS_INTL_DATA_LOCALE), *pool = table(f, QJS_INTL_DATA_UTF8_POOL);
    uint32_t i;
    for (i = 0; i < locales->count; i++) {
        const unsigned char *r = locales->data + (size_t)i * 48;
        if (u32(r + 4) == strlen(tag) && !memcmp(pool->data + u32(r), tag, strlen(tag))) return i;
    }
    assert(0); return 0;
}
static int by_id(const void *a, const void *bptr)
{
    const Table *x = a, *y = bptr; return x->id < y->id ? -1 : x->id > y->id;
}
static unsigned char *assemble(Fixture *f, const QJSIntlDataView *source, size_t *length)
{
    unsigned char *blob;
    size_t size = 64 + f->count * 24, offset, i;
    qsort(f->tables, f->count, sizeof(*f->tables), by_id);
    for (i = 0; i < f->count; i++) {
        size = (size + 3) & ~(size_t)3;
        size += (size_t)f->tables[i].count * f->tables[i].width;
    }
    assert(size <= UINT32_MAX);
    blob = calloc(size, 1); assert(blob);
    memcpy(blob, source->data, 64); blob[10] = 3; blob[11] = 0;
    put(blob + 16, (uint32_t)size); put(blob + 20, (uint32_t)f->count);
    offset = 64 + f->count * 24;
    for (i = 0; i < f->count; i++) {
        Table *t = &f->tables[i];
        size_t bytes_count = (size_t)t->width * t->count;
        unsigned char *d = blob + 64 + i * 24;
        offset = (offset + 3) & ~(size_t)3;
        put(d, t->id); put(d + 4, (uint32_t)offset); put(d + 8, (uint32_t)bytes_count);
        put(d + 12, t->count); put(d + 16, t->width);
        if (bytes_count) memcpy(blob + offset, t->data, bytes_count);
        offset += bytes_count;
    }
    *length = size; return blob;
}
static void clear_fixture(Fixture *f)
{
    size_t i;
    for (i = 0; i < f->count; i++) free(f->tables[i].data);
    memset(f, 0, sizeof(*f));
}
static unsigned char *blob_section(unsigned char *blob, uint32_t id)
{
    uint32_t i, count = u32(blob + 20);
    for (i = 0; i < count; i++) {
        unsigned char *d = blob + 64 + (size_t)i * 24;
        if (u32(d) == id) return blob + u32(d + 4);
    }
    return NULL;
}
static void rejects_word(unsigned char *blob, size_t size, unsigned char *where, uint32_t bad)
{
    QJSIntlDataView out;
    uint32_t saved = u32(where);
    put(where, bad);
    assert(qjs_intl_data_open(blob, size, &out) == QJS_INTL_DATA_INVALID);
    assert(!out.data && !out.length);
    put(where, saved);
}
static unsigned char *installed_fixture(QJSIntlDataView *view, size_t *length)
{
    Fixture f;
    Table *calendars, *info, *zones, *locales;
    uint32_t en, co_first, zone_first;
    Ref gregory, persian, arab, us;
    unsigned char *blob;
    load_fixture(&f, view);
    en = locale_index(&f, "en");
    gregory = pool_add(&f, "gregory"); persian = pool_add(&f, "persian");
    arab = pool_add(&f, "arab"); us = pool_add(&f, "US");
    co_first = list_add(&f, "phonebk"); (void)list_add(&f, "eor"); (void)list_add(&f, "emoji");
    zone_first = list_add(&f, "America/New_York");
    (void)list_add(&f, "America/Los_Angeles"); (void)list_add(&f, "America/New_York");
    calendars = replace_table(&f, QJS_INTL_DATA_AVAILABLE_CALENDAR, 8, 2);
    ref_put(calendars->data, gregory); ref_put(calendars->data + 8, persian);
    info = replace_table(&f, QJS_INTL_DATA_LOCALE_SERVICE_INFO, 28, 2);
    put(info->data, en); put(info->data + 4, 0);
    put(info->data + 16, co_first); put(info->data + 20, 3);
    put(info->data + 28, en); put(info->data + 32, 1); ref_put(info->data + 36, arab);
    zones = replace_table(&f, QJS_INTL_DATA_REGION_TIME_ZONE, 16, 1);
    ref_put(zones->data, us); put(zones->data + 8, zone_first); put(zones->data + 12, 3);
    locales = table(&f, QJS_INTL_DATA_LOCALE);
    put(locales->data + (size_t)en * 48 + 44, (1u << 1) | (1u << 3));
    blob = assemble(&f, view, length); clear_fixture(&f); return blob;
}

int main(int argc, char **argv)
{
    AllocationState state = {0};
    QJSIntlAllocator a = {&state, test_malloc, test_realloc, test_free};
    QJSIntlLocaleInfoCapabilities cap = {0}, installed = {1, 1, 1, 1};
    QJSIntlDataView view, synthetic;
    QJSIntlDataSection scripts;
    QJSIntlLocaleInfoRequest r;
    QJSIntlNativeLocaleInfoResult out;
    unsigned char *blob, *fixture;
    size_t size, fixture_size, i, calls;
    FILE *file;
    long length;
    static const char *const us_hours[] = {"h12", "h23"}, *const gb_hours[] = {"h23", "h12"};
    static const char *const jp_hours[] = {"h23", "h11", "h12"}, *const default_hours[] = {"h23"};
    static const char *const ca_af[] = {"persian", "gregory"}, *const ca_default[] = {"gregory"};
    static const char *const co_en[] = {"emoji", "eor", "phonebk"}, *const co_default[] = {"emoji", "eor"};
    static const char *const nu_en[] = {"arab"}, *const nu_default[] = {"latn"};
    static const char *const zones[] = {"America/Los_Angeles", "America/New_York"};
    assert(argc == 2);
    file = fopen(argv[1], "rb"); assert(file);
    assert(!fseek(file, 0, SEEK_END)); length = ftell(file); assert(length > 0);
    size = (size_t)length; rewind(file); blob = malloc(size); assert(blob);
    assert(fread(blob, 1, size, file) == size); assert(!fclose(file));
    assert(qjs_intl_data_open(blob, size, &view) == QJS_INTL_DATA_OK);

    /* Invalid input never reaches a root/default fallback. */
    {
        const char embedded[] = {'e', 'n', 0, '-', 'U', 'S'};
        const char bounded[] = {'e', 'n', '-', 'U', 'S'};
        const char *invalid[] = {"root", "en_US", "x-private", "en--US", "en-\x80"};
        for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
            r = request(invalid[i]); memset(&out, 0xff, sizeof(out));
            assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_HOUR_CYCLES, &out) == QJS_INTL_INVALID_ARGUMENT);
            zero_result(&out); assert(!state.live);
        }
        r = request("en"); r.locale.data = embedded; r.locale.length = sizeof(embedded);
        assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_HOUR_CYCLES, &out) == QJS_INTL_INVALID_ARGUMENT);
        zero_result(&out);
        r.locale.data = bounded; r.locale.length = sizeof(bounded);
        assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_HOUR_CYCLES, &out) == QJS_INTL_OK);
        check_list(&out, us_hours, 2); qjs_intl_native_locale_info_result_clear(&a, &out);
    }
    list_case(&a, &view, &cap, "EN-us", QJS_INTL_LOCALE_HOUR_CYCLES, us_hours, 2);
    list_case(&a, &view, &cap, "en-GB", QJS_INTL_LOCALE_HOUR_CYCLES, gb_hours, 2);
    list_case(&a, &view, &cap, "ja-JP", QJS_INTL_LOCALE_HOUR_CYCLES, jp_hours, 3);
    list_case(&a, &view, &cap, "en-ZZ", QJS_INTL_LOCALE_HOUR_CYCLES, default_hours, 1);
    list_case(&a, &view, &cap, "en-US-u-rg-gbzzzz", QJS_INTL_LOCALE_HOUR_CYCLES, gb_hours, 2);
    list_case(&a, &view, &cap, "en-US-u-rg-jpx", QJS_INTL_LOCALE_HOUR_CYCLES, jp_hours, 3);
    list_case(&a, &view, &cap, "en-US-u-rg-unknown", QJS_INTL_LOCALE_HOUR_CYCLES, us_hours, 2);
    list_case(&a, &view, &cap, "en-US-u-rg-zzzzzz", QJS_INTL_LOCALE_HOUR_CYCLES, us_hours, 2);
    /* sd supplies the region only when the base lacks one; rg takes priority
     * when its region has preference data. Private/t extension keys ignored. */
    list_case(&a, &view, &cap, "en-u-sd-gbeng", QJS_INTL_LOCALE_HOUR_CYCLES, gb_hours, 2);
    list_case(&a, &view, &cap, "en-US-u-sd-gbeng", QJS_INTL_LOCALE_HOUR_CYCLES, us_hours, 2);
    list_case(&a, &view, &cap, "en-US-x-u-rg-gbzzzz", QJS_INTL_LOCALE_HOUR_CYCLES, us_hours, 2);
    week_case(&a, &view, &cap, "en-US", NULL, 7, 96);
    week_case(&a, &view, &cap, "en-US-u-rg-gbzzzz", NULL, 1, 96);
    week_case(&a, &view, &cap, "en-u-sd-afxyz", NULL, 6, 24);
    week_case(&a, &view, &cap, "en-ZZ", NULL, 1, 96);
    week_case(&a, &view, &cap, "en-US", "tue", 2, 96);
    week_case(&a, &view, &cap, "en-US", "unknown", 7, 96);

    /* Per-method unsupported is explicit. Slots are returned even when the
     * requested well-formed type is unknown or unsupported by a formatter. */
    for (i = 0; i < 4; i++) {
        QJSIntlLocaleInfoField field = (QJSIntlLocaleInfoField)i;
        if (field == QJS_INTL_LOCALE_HOUR_CYCLES) continue;
        r = request("en-US");
        assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, field, &out) == QJS_INTL_UNSUPPORTED);
        zero_result(&out);
    }
    r = request("en-US"); r.calendar = b("unknown");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_CALENDARS, &out) == QJS_INTL_OK);
    assert(out.list.count == 1 && !strcmp(out.list.items[0].data, "unknown"));
    qjs_intl_native_locale_info_result_clear(&a, &out);
    r.collation = b("search");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_COLLATIONS, &out) == QJS_INTL_OK);
    assert(out.list.count == 1 && !strcmp(out.list.items[0].data, "search"));
    qjs_intl_native_locale_info_result_clear(&a, &out);
    r.numbering_system = b("unknown");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_NUMBERING_SYSTEMS, &out) == QJS_INTL_OK);
    qjs_intl_native_locale_info_result_clear(&a, &out);
    r.hour_cycle = b("h11");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_HOUR_CYCLES, &out) == QJS_INTL_OK);
    assert(out.list.count == 1 && !strcmp(out.list.items[0].data, "h11"));
    qjs_intl_native_locale_info_result_clear(&a, &out);
    r.first_day = b("");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_FIRST_DAY_OF_WEEK, &out) == QJS_INTL_OK);
    assert(out.defined && out.value && !*out.value); qjs_intl_native_locale_info_result_clear(&a, &out);
    r.first_day = b("tue");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_FIRST_DAY_OF_WEEK, &out) == QJS_INTL_OK);
    assert(out.defined && !strcmp(out.value, "tue")); qjs_intl_native_locale_info_result_clear(&a, &out);
    r = request("en-u-fw-tue"); /* slots must be supplied by frontend */
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_FIRST_DAY_OF_WEEK, &out) == QJS_INTL_OK);
    assert(!out.defined && !out.value);
    r.hour_cycle.data = NULL; r.hour_cycle.length = 3;
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_HOUR_CYCLES, &out) == QJS_INTL_INVALID_ARGUMENT);
    zero_result(&out);
    r = request("en-US"); r.hour_cycle = b("h99");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_HOUR_CYCLES, &out) == QJS_INTL_OK);
    assert(out.defined && out.list.count == 1 && !strcmp(out.list.items[0].data, "h99"));
    qjs_intl_native_locale_info_result_clear(&a, &out);
    r.hour_cycle = b("unknown");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_HOUR_CYCLES, &out) == QJS_INTL_OK);
    assert(out.list.count == 1 && !strcmp(out.list.items[0].data, "unknown"));
    qjs_intl_native_locale_info_result_clear(&a, &out);
    r.hour_cycle = b("not_valid");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_HOUR_CYCLES, &out) == QJS_INTL_INVALID_ARGUMENT);
    zero_result(&out);

    r = request("en-u-rg-uszzzz-sd-usca");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_TIME_ZONES, &out) == QJS_INTL_OK);
    assert(!out.defined && !out.list.count);
    r = request("en-US");
    assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_TIME_ZONES, &out) == QJS_INTL_UNSUPPORTED);

    if (qjs_intl_data_section(&view, QJS_INTL_DATA_SCRIPT_DIRECTION, &scripts) == QJS_INTL_DATA_OK) {
        static const struct { const char *tag; unsigned int defined; QJSIntlLocaleDirection direction; } text_cases[] = {
            {"en", 1, QJS_INTL_LOCALE_DIRECTION_LTR}, {"ar", 1, QJS_INTL_LOCALE_DIRECTION_RTL},
            {"ar-Latn", 1, QJS_INTL_LOCALE_DIRECTION_LTR}, {"en-Arab", 1, QJS_INTL_LOCALE_DIRECTION_RTL},
            {"en-Qaaa", 0, QJS_INTL_LOCALE_DIRECTION_UNDEFINED},
            {"en-Zyyy", 0, QJS_INTL_LOCALE_DIRECTION_UNDEFINED},
            {"en-Brai", 0, QJS_INTL_LOCALE_DIRECTION_UNDEFINED},
            {"en-u-sd-afxyz", 1, QJS_INTL_LOCALE_DIRECTION_LTR}
        };
        for (i = 0; i < sizeof(text_cases) / sizeof(*text_cases); i++) {
            r = request(text_cases[i].tag);
            assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_TEXT_INFO, &out) == QJS_INTL_OK);
            assert(out.defined == text_cases[i].defined && out.direction == text_cases[i].direction);
            qjs_intl_native_locale_info_result_clear(&a, &out);
        }
    } else {
        r = request("en");
        assert(qjs_intl_native_locale_info_get(&a, &view, &cap, &r, QJS_INTL_LOCALE_TEXT_INFO, &out) == QJS_INTL_UNSUPPORTED);
        zero_result(&out);
    }

    fixture = installed_fixture(&view, &fixture_size);
    assert(qjs_intl_data_open(fixture, fixture_size, &synthetic) == QJS_INTL_DATA_OK);
    {
        unsigned char *info = blob_section(fixture, QJS_INTL_DATA_LOCALE_SERVICE_INFO);
        unsigned char *calendars = blob_section(fixture, QJS_INTL_DATA_AVAILABLE_CALENDAR);
        unsigned char *locales = blob_section(fixture, QJS_INTL_DATA_LOCALE);
        unsigned char *directions = blob_section(fixture, QJS_INTL_DATA_SCRIPT_DIRECTION);
        uint32_t en = u32(info);
        rejects_word(fixture, fixture_size, info + 4, 2); /* unknown service */
        rejects_word(fixture, fixture_size, info + 24, 1); /* flags reserved */
        rejects_word(fixture, fixture_size, locales + (size_t)en * 48 + 44, 0); /* false coverage */
        rejects_word(fixture, fixture_size, info + 36, 0); /* nonempty default ref required */
        rejects_word(fixture, fixture_size, info + 32, 0); /* duplicate service/locale key */
        rejects_word(fixture, fixture_size, calendars, u32(calendars + 8)); /* corrupt reference */
        if (directions) rejects_word(fixture, fixture_size, directions + 8, 3);
        fixture[10] = 2;
        assert(qjs_intl_data_open(fixture, fixture_size, &synthetic) == QJS_INTL_DATA_INVALID);
        fixture[10] = 4;
        assert(qjs_intl_data_open(fixture, fixture_size, &synthetic) == QJS_INTL_DATA_UNSUPPORTED_VERSION);
        fixture[10] = 3;
        assert(qjs_intl_data_open(fixture, fixture_size, &synthetic) == QJS_INTL_DATA_OK);
    }
    list_case(&a, &synthetic, &installed, "fa-AF", QJS_INTL_LOCALE_CALENDARS, ca_af, 2);
    list_case(&a, &synthetic, &installed, "en-ZZ", QJS_INTL_LOCALE_CALENDARS, ca_default, 1);
    list_case(&a, &synthetic, &installed, "en-US-u-rg-afxyz", QJS_INTL_LOCALE_CALENDARS, ca_af, 2);
    list_case(&a, &synthetic, &installed, "en-US", QJS_INTL_LOCALE_COLLATIONS, co_en, 3);
    list_case(&a, &synthetic, &installed, "en-US-u-co-search-x-zz", QJS_INTL_LOCALE_COLLATIONS, co_en, 3);
    list_case(&a, &synthetic, &installed, "de-DE", QJS_INTL_LOCALE_COLLATIONS, co_default, 2);
    list_case(&a, &synthetic, &installed, "en-US", QJS_INTL_LOCALE_NUMBERING_SYSTEMS, nu_en, 1);
    list_case(&a, &synthetic, &installed, "de-DE", QJS_INTL_LOCALE_NUMBERING_SYSTEMS, nu_default, 1);
    list_case(&a, &synthetic, &installed, "en-US-u-rg-gbzzzz", QJS_INTL_LOCALE_TIME_ZONES, zones, 2);
    list_case(&a, &synthetic, &installed, "en-GB", QJS_INTL_LOCALE_TIME_ZONES, NULL, 0);

    /* Fail every allocation in paths that own a list, scalar string, likely
     * subtags and a calendar filter. Output is zero, allocations reclaimed. */
    for (i = 0; i < 5; i++) {
        QJSIntlLocaleInfoField field = i == 0 ? QJS_INTL_LOCALE_CALENDARS :
            i == 1 ? QJS_INTL_LOCALE_COLLATIONS : i == 2 ? QJS_INTL_LOCALE_HOUR_CYCLES :
            i == 3 ? QJS_INTL_LOCALE_TIME_ZONES : QJS_INTL_LOCALE_FIRST_DAY_OF_WEEK;
        size_t fail;
        r = request(i == 0 ? "fa-u-sd-afxyz" : "en-US-u-rg-gbzzzz");
        if (i == 4) r.first_day = b("tue");
        state.calls = 0;
        assert(qjs_intl_native_locale_info_get(&a, &synthetic, &installed, &r, field, &out) == QJS_INTL_OK);
        calls = state.calls; qjs_intl_native_locale_info_result_clear(&a, &out); assert(!state.live);
        for (fail = 1; fail <= calls; fail++) {
            state.calls = 0; state.fail_at = fail; memset(&out, 0xff, sizeof(out));
            assert(qjs_intl_native_locale_info_get(&a, &synthetic, &installed, &r, field, &out) == QJS_INTL_NO_MEMORY);
            zero_result(&out); assert(!state.live);
            qjs_intl_native_locale_info_result_clear(&a, &out);
            state.fail_at = 0;
        }
    }
    qjs_intl_native_locale_info_result_clear(&a, NULL);
    free(fixture); free(blob); assert(!state.live);
    return 0;
}
