/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Source-only harness: root runs this against the assembled source packet.
 */
#include "intl/list-native.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct FailAllocator { size_t calls, fail_at, live; } FailAllocator;
static void *fm(void *opaque, size_t bytes)
{
    FailAllocator *s = opaque;
    void *p;
    assert(bytes);
    if (++s->calls == s->fail_at) return NULL;
    p = malloc(bytes);
    if (p) s->live++;
    return p;
}
static void ff(void *opaque, void *p)
{
    FailAllocator *s = opaque;
    if (p) { assert(s->live); s->live--; free(p); }
}
static QJSIntlAllocator allocator(FailAllocator *s)
{
    QJSIntlAllocator a = { s, fm, NULL, ff };
    return a;
}
static QJSIntlBytes ascii(const char *s)
{
    QJSIntlBytes value = { s, strlen(s) };
    return value;
}
static QJSIntlListTemplates simple(const char *pair, const char *start,
                                   const char *middle, const char *end)
{
    QJSIntlListTemplates t;
    memset(&t, 0, sizeof(t));
    t.base[0] = ascii(pair); t.base[1] = ascii(start);
    t.base[2] = ascii(middle); t.base[3] = ascii(end);
    return t;
}
static void text_is(const QJSIntlFormatted *out, const char *text)
{
    size_t i;
    assert(out->length == strlen(text));
    for (i = 0; i < out->length; i++) assert(out->text[i] == (unsigned char)text[i]);
}
static void partition(const QJSIntlFormatted *out)
{
    size_t i, cursor = 0;
    for (i = 0; i < out->part_count; i++) {
        assert(out->parts[i].start == cursor);
        assert(out->parts[i].end >= cursor && out->parts[i].end <= out->length);
        cursor = out->parts[i].end;
        assert(out->parts[i].source == QJS_INTL_SOURCE_SINGLE);
        assert(out->parts[i].unit.data == NULL && out->parts[i].unit.length == 0);
    }
    assert(cursor == out->length);
}
static void generic(void)
{
    FailAllocator s = {0};
    QJSIntlAllocator a = allocator(&s);
    QJSIntlListTemplates t = simple("{0}P{1}", "S{0}/{1}s", "M{0}+{1}m", "E{0}&{1}e");
    QJSIntlNativeList *list;
    QJSIntlFormatted out;
    uint16_t letters[] = {'A', 'B', 'C', 'D'};
    QJSIntlUTF16 items[4] = {{letters,1},{letters+1,1},{letters+2,1},{letters+3,1}};
    assert(qjs_intl_native_list_open(&a, &t, &list) == QJS_INTL_OK);
    assert(qjs_intl_native_list_format(list, NULL, 0, &out) == QJS_INTL_OK);
    assert(!out.text && !out.parts && !out.length && !out.part_count);
    assert(qjs_intl_native_list_format(list, items, 1, &out) == QJS_INTL_OK);
    text_is(&out, "A"); assert(out.part_count == 1); partition(&out);
    qjs_intl_native_list_result_clear(&a, &out);
    assert(qjs_intl_native_list_format(list, items, 2, &out) == QJS_INTL_OK);
    text_is(&out, "APB"); assert(out.part_count == 3); partition(&out);
    qjs_intl_native_list_result_clear(&a, &out);
    assert(qjs_intl_native_list_format(list, items, 4, &out) == QJS_INTL_OK);
    text_is(&out, "SA/MB+EC&Dems"); assert(out.part_count == 13); partition(&out);
    qjs_intl_native_list_result_clear(&a, &out);
    qjs_intl_native_list_close(list); assert(!s.live);
}
static void exact_elements(void)
{
    FailAllocator s = {0}; QJSIntlAllocator a = allocator(&s);
    QJSIntlListTemplates t = simple("{1}|{0}", "{1}/{0}", "{1}+{0}", "{1}&{0}");
    QJSIntlNativeList *list; QJSIntlFormatted out;
    uint16_t unusual[] = {0, 0xd800, 0xdc00, 0xdfff};
    QJSIntlUTF16 items[] = {{NULL,0},{unusual,4},{NULL,0}};
    assert(qjs_intl_native_list_open(&a, &t, &list) == QJS_INTL_OK);
    assert(qjs_intl_native_list_format(list, items, 3, &out) == QJS_INTL_OK);
    assert(out.length == 6 && out.part_count == 5); partition(&out);
    assert(out.parts[0].type == QJS_INTL_PART_ELEMENT && out.parts[0].start == out.parts[0].end);
    assert(out.text[0] == '&' && memcmp(out.text+1, unusual, sizeof(unusual)) == 0 && out.text[5] == '/');
    assert(out.parts[4].type == QJS_INTL_PART_ELEMENT && out.parts[4].start == out.parts[4].end);
    qjs_intl_native_list_result_clear(&a, &out);
    assert(qjs_intl_native_list_format(list, items, 1, &out) == QJS_INTL_OK);
    assert(!out.text && out.part_count == 1 && out.parts[0].start == 0 && out.parts[0].end == 0);
    qjs_intl_native_list_result_clear(&a, &out);
    qjs_intl_native_list_result_clear(&a, &out);
    items[0].length = SIZE_MAX;
    assert(qjs_intl_native_list_format(list, items, 1, &out) == QJS_INTL_INVALID_ARGUMENT);
    items[0].data = unusual;
    assert(qjs_intl_native_list_format(list, items, 1, &out) == QJS_INTL_OVERFLOW);
    assert(qjs_intl_native_list_format(list, items, SIZE_MAX, &out) == QJS_INTL_OVERFLOW);
    qjs_intl_native_list_close(list); assert(!s.live);
}
static void contexts(void)
{
    static const uint16_t first[] = {'A'};
    static const QJSIntlListScriptRange ranges[] = {{0x591,0x5c9},{0x5d0,0x5ea},{0xfb1d,0xfb36}};
    static const struct { const char *word; int e, u; } cases[] = {
        {"",0,0},{"ibis",1,0},{"I",1,0},{"HI",1,0},{"hilo",1,0},
        {"hielo",0,0},{"HIA",0,0},{"a",0,0},{"o",0,1},{"HO",0,1},
        {"8",0,1},{"80",0,1},{"11",0,1},{"11 a",0,1},{"110",0,0},{"11\ta",0,0}};
    FailAllocator s = {0}; QJSIntlAllocator a = allocator(&s);
    QJSIntlNativeList *list; QJSIntlFormatted out;
    QJSIntlListTemplates t;
    QJSIntlUTF16 items[3] = {{first,1},{first,1},{NULL,0}};
    uint16_t word[32]; size_t i, j; int context;
    for (context = 1; context <= 2; context++) {
        t = simple("{0} y {1}", "{0}, {1}", "{0}, {1}", "{0} y {1}");
        t.context = (QJSIntlListContext)context;
        if (context == 2) t.base[0] = t.base[3] = ascii("{0} o {1}");
        for (i = 0; i < 4; i++) t.alternate[i] = t.base[i];
        t.alternate[0] = t.alternate[3] = ascii(context == 1 ? "{0} e {1}" : "{0} u {1}");
        assert(qjs_intl_native_list_open(&a, &t, &list) == QJS_INTL_OK);
        for (i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
            size_t count;
            for (j = 0; cases[i].word[j]; j++) word[j] = (unsigned char)cases[i].word[j];
            for (count = 2; count <= 3; count++) {
                int alternate = context == 1 ? cases[i].e : cases[i].u;
                items[count-1].data = word; items[count-1].length = j;
                assert(qjs_intl_native_list_format(list, items, count, &out) == QJS_INTL_OK);
                assert(out.text[count == 2 ? 2 : 5] ==
                       (alternate ? (context == 1 ? 'e' : 'u') : (context == 1 ? 'y' : 'o')));
                partition(&out); qjs_intl_native_list_result_clear(&a, &out);
                items[1].data = first; items[1].length = 1;
            }
        }
        qjs_intl_native_list_close(list);
    }
    t = simple("{0} \xd7\x95{1}", "{0}, {1}", "{0}, {1}", "{0} \xd7\x95{1}");
    t.context = QJS_INTL_LIST_CONTEXT_HEBREW_AND;
    for (i = 0; i < 4; i++) t.alternate[i] = t.base[i];
    t.alternate[0] = t.alternate[3] = ascii("{0} \xd7\x95-{1}");
    t.hebrew_script = ranges; t.hebrew_script_count = sizeof(ranges)/sizeof(ranges[0]);
    assert(qjs_intl_native_list_open(&a, &t, &list) == QJS_INTL_OK);
    { const uint16_t starts[] = {0x5d0,0x591,0x5c8,0xfb1d,0x5cb,'A',0,0xd800};
      for (i = 0; i < sizeof(starts)/sizeof(starts[0]); i++) {
        items[1].data = starts+i; items[1].length = 1;
        assert(qjs_intl_native_list_format(list, items, 2, &out) == QJS_INTL_OK);
        assert((out.text[3] == '-') == (i >= 4));
        partition(&out); qjs_intl_native_list_result_clear(&a, &out);
      }
    }
    items[1].data = NULL; items[1].length = 0;
    assert(qjs_intl_native_list_format(list, items, 2, &out) == QJS_INTL_OK);
    assert(out.length == 3); qjs_intl_native_list_result_clear(&a, &out);
    qjs_intl_native_list_close(list); assert(!s.live);
}
static void snapshot_and_long_list(void)
{
    FailAllocator s = {0}; QJSIntlAllocator a = allocator(&s);
    char pair[] = "\xf0\x9f\x98\x80{0}\xe2\x80\x8f{1}";
    QJSIntlListTemplates t = simple(pair, "{0}/{1}", "{0}+{1}", "{0}&{1}");
    QJSIntlNativeList *list; QJSIntlFormatted out;
    QJSIntlUTF16 *items;
    uint16_t letter = 'A'; size_t i, elements = 0;
    assert(qjs_intl_native_list_open(&a, &t, &list) == QJS_INTL_OK);
    memset(pair, 'x', sizeof(pair)-1);
    items = malloc(4096 * sizeof(*items)); assert(items);
    for (i = 0; i < 4096; i++) { items[i].data = &letter; items[i].length = 1; }
    assert(qjs_intl_native_list_format(list, items, 2, &out) == QJS_INTL_OK);
    assert(out.length == 5 && out.text[0] == 0xd83d && out.text[1] == 0xde00 &&
           out.text[2] == 'A' && out.text[3] == 0x200f && out.text[4] == 'A');
    qjs_intl_native_list_result_clear(&a, &out);
    assert(qjs_intl_native_list_format(list, items, 4096, &out) == QJS_INTL_OK);
    assert(out.length == 8191 && out.part_count == 8191); partition(&out);
    for (i = 0; i < out.part_count; i++) elements += out.parts[i].type == QJS_INTL_PART_ELEMENT;
    assert(elements == 4096);
    qjs_intl_native_list_result_clear(&a, &out);
    qjs_intl_native_list_close(list); free(items); assert(!s.live);
}
static void failures(void)
{
    QJSIntlListTemplates t = simple("'{1}\xc2\xa0{0}'", "{0}/{1}", "{0}+{1}", "{0}&{1}");
    uint16_t letters[] = {'A','B'};
    QJSIntlUTF16 items[] = {{letters,1},{letters+1,1}};
    size_t fail;
    for (fail = 1; fail <= 10; fail++) {
        FailAllocator s = {0}; QJSIntlAllocator a = allocator(&s);
        QJSIntlNativeList *list = NULL; QJSIntlFormatted out;
        QJSIntlStatus status;
        s.fail_at = fail;
        status = qjs_intl_native_list_open(&a, &t, &list);
        if (status == QJS_INTL_OK) {
            status = qjs_intl_native_list_format(list, items, 2, &out);
            if (status == QJS_INTL_OK) {
                assert(out.length == 5 && out.text[0] == '\'' && out.text[1] == 'B' &&
                       out.text[2] == 0xa0 && out.text[3] == 'A' && out.text[4] == '\'');
                partition(&out);
            } else assert(status == QJS_INTL_NO_MEMORY && !out.text && !out.parts && !out.part_count);
            qjs_intl_native_list_result_clear(&a, &out);
            qjs_intl_native_list_close(list);
        } else assert(status == QJS_INTL_NO_MEMORY && !list);
        assert(!s.live);
    }
    { const char *bad[] = {"{0}","{0}{0}{1}","{0}{2}","{0}{1}}", "\xed\xa0\x80{0}{1}",
                          "\xc0\x80{0}{1}","{0}{1}\xe2", "\xf4\x90\x80\x80{0}{1}"};
      for (fail = 0; fail < sizeof(bad)/sizeof(bad[0]); fail++) {
        FailAllocator s = {0}; QJSIntlAllocator a = allocator(&s); QJSIntlNativeList *list;
        t.base[0] = ascii(bad[fail]);
        assert(qjs_intl_native_list_open(&a, &t, &list) == QJS_INTL_DATA_ERROR && !list && !s.live);
      }
    }
    { FailAllocator s = {0}; QJSIntlAllocator a = allocator(&s); QJSIntlNativeList *list;
      t.base[0].data = "{0}\0{1}"; t.base[0].length = 7;
      assert(qjs_intl_native_list_open(&a, &t, &list) == QJS_INTL_DATA_ERROR && !list && !s.live);
    }
    t = simple("{0} \xd7\x95{1}", "{0}, {1}", "{0}, {1}", "{0} \xd7\x95{1}");
    t.context = QJS_INTL_LIST_CONTEXT_HEBREW_AND;
    { static const QJSIntlListScriptRange ranges[] = {{0x591,0x5c9},{0x5d0,0x5ea}};
      size_t i;
      t.hebrew_script = ranges; t.hebrew_script_count = 2;
      for (i = 0; i < 4; i++) t.alternate[i] = t.base[i];
      t.alternate[0] = t.alternate[3] = ascii("{0} \xd7\x95-{1}");
      for (fail = 1; fail <= 15; fail++) {
        FailAllocator s = {0}; QJSIntlAllocator a = allocator(&s);
        QJSIntlNativeList *list = NULL; QJSIntlFormatted out;
        QJSIntlStatus status;
        s.fail_at = fail;
        status = qjs_intl_native_list_open(&a, &t, &list);
        if (status == QJS_INTL_OK) {
            status = qjs_intl_native_list_format(list, items, 2, &out);
            if (status != QJS_INTL_OK)
                assert(status == QJS_INTL_NO_MEMORY && !out.text && !out.parts);
            qjs_intl_native_list_close(list);
            /* The result owns its memory and survives handle close. */
            qjs_intl_native_list_result_clear(&a, &out);
        } else assert(status == QJS_INTL_NO_MEMORY && !list);
        assert(!s.live);
      }
      { static const QJSIntlListScriptRange bad_ranges[][2] = {
            {{0x591,0x5c9},{0x5c9,0x5ea}}, {{0x591,0x5c9},{0xd800,0xd800}},
            {{0x5c9,0x591},{0x5d0,0x5ea}}, {{0x591,0x5c9},{0x110000,0x110000}}};
        for (i = 0; i < sizeof(bad_ranges)/sizeof(bad_ranges[0]); i++) {
            FailAllocator s = {0}; QJSIntlAllocator a = allocator(&s); QJSIntlNativeList *list;
            t.hebrew_script = bad_ranges[i];
            assert(qjs_intl_native_list_open(&a, &t, &list) == QJS_INTL_DATA_ERROR && !list && !s.live);
        }
      }
    }
}
int main(void)
{
    generic(); exact_elements(); contexts(); snapshot_and_long_list(); failures();
    return 0;
}
