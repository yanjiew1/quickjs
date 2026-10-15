/* Native Number handle/provider lifetime, binary snapshot and allocation gates.
 * Copyright (c) 2026 Yan-Jie Wang. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "../src/intl/provider-native-number.h"
#include "../src/intl/data/locale-metadata.h"
#include "../src/intl/data/number-extra-schema.h"
typedef struct Heap { size_t calls, failure, live; } Heap;
static void *allocate(void *opaque, size_t n)
{
    Heap *h = opaque; void *p;
    if (++h->calls == h->failure) return NULL;
    p = malloc(n); if (p) h->live++; return p;
}
static void release(void *opaque, void *p)
{
    Heap *h = opaque;
    if (p) { assert(h->live); h->live--; free(p); }
}
static void *resize(void *opaque, void *p, size_t n)
{
    Heap *h = opaque; void *result;
    if (!p) return allocate(opaque,n);
    if (!n) { release(opaque,p); return NULL; }
    if (++h->calls == h->failure) return NULL;
    result = realloc(p,n); return result;
}
static QJSIntlBytes text(const char *s) { return (QJSIntlBytes){s,strlen(s)}; }
static QJSIntlNumberOptions options(void)
{
    QJSIntlNumberOptions o = {0};
    o.grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    o.maximum_output_length = o.digits.maximum_output_length = 4096;
    o.digits.minimum_integer_digits = 1;
    o.digits.maximum_fraction_digits = 3;
    o.digits.minimum_significant_digits = 1;
    o.digits.maximum_significant_digits = 21;
    o.digits.rounding_increment = 1;
    o.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    return o;
}
static QJSIntlProvider *provider(const QJSIntlAllocator *a)
{
    QJSIntlProviderConfig config = {0}; QJSIntlProvider *p = NULL;
    config.backend = QJS_INTL_BACKEND_NATIVE; config.allocator = *a;
    config.default_locale = text("en-US"); config.default_time_zone = text("UTC");
    assert(qjs_intl_provider_new(&config,&p) == QJS_INTL_OK && p); return p;
}
static uint32_t index_of(const QJSIntlDataView *v, uint32_t id, const char *name)
{
    QJSIntlDataSection s; QJSIntlDataSlice value; uint32_t i;
    assert(qjs_intl_data_section(v,id,&s) == QJS_INTL_DATA_OK);
    for (i=0;i<s.record_count;i++) {
        assert(qjs_intl_data_record_string(v,&s,i,0,&value) == QJS_INTL_DATA_OK);
        if (value.length == strlen(name) && !memcmp(value.data,name,value.length)) return i;
    }
    assert(0); return 0;
}
static QJSIntlFormatted format(const QJSIntlNativeNumber *n)
{
    QJSIntlMathematicalValue value = {QJS_INTL_FINITE,{"12345.5",7}};
    QJSIntlFormatted f = {0}; size_t i,cursor=0;
    assert(qjs_intl_native_number_format(n,&value,&f) == QJS_INTL_OK);
    assert(f.length && f.text && f.part_count && f.parts);
    for(i=0;i<f.part_count;i++) {
        assert(f.parts[i].start == cursor && f.parts[i].end > cursor);
        assert(f.parts[i].source == QJS_INTL_SOURCE_SINGLE); cursor=f.parts[i].end;
    }
    assert(cursor == f.length); return f;
}
static void equal(const QJSIntlFormatted *a, const QJSIntlFormatted *b)
{
    size_t i;
    assert(a->length == b->length && a->part_count == b->part_count);
    assert(!memcmp(a->text,b->text,a->length*sizeof(*a->text)));
    for(i=0;i<a->part_count;i++) {
        assert(a->parts[i].start == b->parts[i].start && a->parts[i].end == b->parts[i].end);
        assert(a->parts[i].type == b->parts[i].type && a->parts[i].source == b->parts[i].source);
    }
}
static void provider_lifetime(const QJSIntlAllocator *a)
{
    QJSIntlProvider *p = provider(a);
    QJSIntlNumberOptions o = options(), resolved;
    QJSIntlNativeNumber *n = NULL;
    QJSIntlFormatted before,after;
    char currency[]="USD";
    o.style=QJS_INTL_NUMBER_CURRENCY; o.currency_display=QJS_INTL_CURRENCY_NAME; o.currency=text(currency);
    assert(qjs_intl_native_provider_number_open(p,text("en-US"),text("latn"),&o,&n) == QJS_INTL_OK && n);
    before=format(n);
    memcpy(currency,"XXX",3); qjs_intl_provider_free(p);
    /* The handle's copied allocator remains alive; provider is already gone. */
    assert(qjs_intl_native_number_resolved_options(n,&resolved) == QJS_INTL_OK);
    assert(resolved.currency.length == 3 && !memcmp(resolved.currency.data,"USD",3));
    after=format(n); equal(&before,&after);
    qjs_intl_native_number_result_clear(n,&before); qjs_intl_native_number_result_clear(n,&after);
    qjs_intl_native_number_close(n);
}
static void binary_snapshot(const QJSIntlAllocator *a, unsigned int mode)
{
    unsigned char *copy=malloc(qjs_intl_locale_metadata_blob_size);
    QJSIntlDataView view;
    QJSIntlNumberOptions o=options(); QJSIntlNativeNumber *n=NULL;
    QJSIntlFormatted before,after; uint32_t en,latn;
    assert(copy); memcpy(copy,qjs_intl_locale_metadata_blob,qjs_intl_locale_metadata_blob_size);
    assert(qjs_intl_data_open(copy,qjs_intl_locale_metadata_blob_size,&view) == QJS_INTL_DATA_OK);
    en=index_of(&view,QJS_INTL_DATA_LOCALE,"en"); latn=index_of(&view,QJS_INTL_DATA_NUMBERING,"latn");
    if(mode==0) {o.notation=QJS_INTL_NUMBER_COMPACT; o.compact_display=QJS_INTL_COMPACT_LONG;}
    if(mode==1) {o.style=QJS_INTL_NUMBER_UNIT;o.unit=text("meter-per-second");o.unit_display=QJS_INTL_UNIT_LONG;}
    if(mode==2) {o.style=QJS_INTL_NUMBER_CURRENCY;o.currency=text("USD");o.currency_display=QJS_INTL_CURRENCY_NAME;}
    assert(qjs_intl_native_number_open_data(a,&view,en,latn,&o,&n) == QJS_INTL_OK && n);
    before=format(n); memset(copy,0,qjs_intl_locale_metadata_blob_size); free(copy);
    after=format(n); equal(&before,&after);
    qjs_intl_native_number_result_clear(n,&before);qjs_intl_native_number_result_clear(n,&after);
    qjs_intl_native_number_close(n);
}
static void installed_digest(const QJSIntlAllocator *a)
{
    QJSIntlProvider *p = provider(a);
    const QJSIntlDataVersions *versions = qjs_intl_provider_versions(p);
    QJSIntlDataSection digest;
    size_t i;
    const char hex[] = "0123456789abcdef";
    assert(versions && versions->input_manifest_sha256.length == 64);
    assert(qjs_intl_data_section(qjs_intl_native_provider_view(p),
        QJS_INTL_DATA_INPUT_SHA256, &digest) == QJS_INTL_DATA_OK);
    assert(digest.bytes.length == 32);
    for (i = 0; i < 32; i++) {
        assert(versions->input_manifest_sha256.data[i * 2] == hex[digest.bytes.data[i] >> 4]);
        assert(versions->input_manifest_sha256.data[i * 2 + 1] == hex[digest.bytes.data[i] & 15]);
    }
    qjs_intl_provider_free(p);
}
static uint32_t read32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void write32(unsigned char *p, uint32_t v)
{
    unsigned int i;
    for (i = 0; i < 4; i++) p[i] = (unsigned char)(v >> (8 * i));
}
/* Preserve offsets and clear the removed bytes, including directory padding. */
static void remove_section(unsigned char *blob, uint32_t id)
{
    uint32_t count = read32(blob + QJS_INTL_H_SECTION_COUNT), i;
    unsigned char *directory = blob + QJS_INTL_DATA_HEADER_SIZE;
    for (i = 0; i < count; i++) {
        unsigned char *entry = directory + i * QJS_INTL_DATA_DIRECTORY_RECORD_SIZE;
        if (read32(entry) != id) continue;
        memset(blob + read32(entry + QJS_INTL_D_OFFSET), 0,
               read32(entry + QJS_INTL_D_BYTE_LENGTH));
        memmove(entry, entry + QJS_INTL_DATA_DIRECTORY_RECORD_SIZE,
                (count - i - 1) * QJS_INTL_DATA_DIRECTORY_RECORD_SIZE);
        memset(directory + (count - 1) * QJS_INTL_DATA_DIRECTORY_RECORD_SIZE,
               0, QJS_INTL_DATA_DIRECTORY_RECORD_SIZE);
        write32(blob + QJS_INTL_H_SECTION_COUNT, count - 1);
        return;
    }
    assert(0);
}
static void pool_reference(const QJSIntlDataView *view, const char *text_value,
                            unsigned char out[8])
{
    QJSIntlDataSection pool;
    size_t at = 0, length = strlen(text_value);
    assert(qjs_intl_data_section(view, QJS_INTL_DATA_UTF8_POOL, &pool) == QJS_INTL_DATA_OK);
    while (at < pool.bytes.length) {
        size_t size = strlen((const char *)pool.bytes.data + at);
        if (size == length && !memcmp(pool.bytes.data + at, text_value, size)) {
            write32(out, (uint32_t)at); write32(out + 4, (uint32_t)size); return;
        }
        at += size + 1;
    }
    assert(0);
}
static void named_compound_data(const QJSIntlAllocator *a)
{
    unsigned char *copy = malloc(qjs_intl_locale_metadata_blob_size);
    QJSIntlDataView view, rejected;
    QJSIntlDataSection named;
    QJSIntlNativeNumber *number = NULL;
    QJSIntlNumberOptions o = options();
    QJSIntlFormatted value;
    uint32_t en, latn;
    unsigned int mode;
    assert(copy);
    o.style = QJS_INTL_NUMBER_UNIT; o.unit = text("meter-per-second");
    o.unit_display = QJS_INTL_UNIT_LONG;
    for (mode = 0; mode < 3; mode++) {
        memcpy(copy, qjs_intl_locale_metadata_blob, qjs_intl_locale_metadata_blob_size);
        if (!mode) {
            remove_section(copy, QJS_INTL_DATA_NUMBER_UNIT);
            remove_section(copy, QJS_INTL_DATA_NUMBER_DENOMINATOR);
            remove_section(copy, QJS_INTL_DATA_NUMBER_COMPOUND_PER);
        } else if (mode == 1) {
            remove_section(copy, QJS_INTL_DATA_NUMBER_NAMED_COMPOUND);
        }
        assert(qjs_intl_data_open(copy, qjs_intl_locale_metadata_blob_size, &view) == QJS_INTL_DATA_OK);
        en = index_of(&view, QJS_INTL_DATA_LOCALE, "en");
        latn = index_of(&view, QJS_INTL_DATA_NUMBERING, "latn");
        if (mode == 2) {
            unsigned char ref[8]; uint32_t row;
            pool_reference(&view, "{number}", ref);
            assert(qjs_intl_data_section(&view, QJS_INTL_DATA_NUMBER_NAMED_COMPOUND, &named) == QJS_INTL_DATA_OK);
            /* All named templates may omit their affix, independently of plural. */
            for (row = 0; row < named.record_count; row++) {
                unsigned int category;
                unsigned char *record = copy + (named.bytes.data - view.data) + row * 64;
                for (category = 0; category < 6; category++) memcpy(record + 16 + category * 8, ref, 8);
            }
            assert(qjs_intl_data_open(copy, qjs_intl_locale_metadata_blob_size, &view) == QJS_INTL_DATA_OK);
        }
        assert(qjs_intl_native_number_open_data(a, &view, en, latn, &o, &number) == QJS_INTL_OK && number);
        value = format(number);
        {
            const char *expected = mode == 2 ? "12,345.5" : "12,345.5 meters per second";
            size_t character;
            assert(value.length == strlen(expected));
            for (character = 0; character < value.length; character++)
                assert(value.text[character] == (unsigned char)expected[character]);
        }
        qjs_intl_native_number_result_clear(number, &value);
        qjs_intl_native_number_close(number); number = NULL;
    }
    for (mode = 0; mode < 8; mode++) {
        unsigned char *last;
        memcpy(copy, qjs_intl_locale_metadata_blob, qjs_intl_locale_metadata_blob_size);
        assert(qjs_intl_data_open(copy, qjs_intl_locale_metadata_blob_size, &view) == QJS_INTL_DATA_OK);
        en = index_of(&view, QJS_INTL_DATA_LOCALE, "en");
        latn = index_of(&view, QJS_INTL_DATA_NUMBERING, "latn");
        assert(qjs_intl_data_section(&view, QJS_INTL_DATA_NUMBER_NAMED_COMPOUND, &named) == QJS_INTL_DATA_OK);
        assert(named.record_count > 1);
        last = copy + (named.bytes.data - view.data) + (named.record_count - 1) * 64;
        /* Corrupt an unselected locale: ordinary construction still scans it. */
        if (mode == 0) last[13] = 1;
        if (mode == 1) last[12] = 3;
        if (mode == 2) write32(last, UINT32_MAX);
        if (mode == 3) pool_reference(&view, "{number}", last + 4);
        if (mode == 4) write32(last + 16, UINT32_MAX);
        if (mode == 5) pool_reference(&view, "{numerator}/{denominator}", last + 16);
        if (mode == 6) memcpy(last, last - 64, 16);
        if (mode == 7) pool_reference(&view, "meter", last + 4);
        assert(qjs_intl_native_number_open_data(a, &view, en, latn, &o, &number) == QJS_INTL_DATA_ERROR && !number);
        assert(qjs_intl_data_open(copy, qjs_intl_locale_metadata_blob_size, &rejected) == QJS_INTL_DATA_INVALID);
        assert(!rejected.data && !rejected.length);
    }
    free(copy);
}

static void allocation_sweep(const QJSIntlAllocator *a, Heap *h)
{
    QJSIntlProvider *p=provider(a);
    QJSIntlNumberOptions o=options();QJSIntlNativeNumber *n=NULL;
    size_t count,i,live=h->live; QJSIntlStatus status;
    o.style=QJS_INTL_NUMBER_UNIT;o.unit=text("meter-per-second");o.unit_display=QJS_INTL_UNIT_LONG;
    h->calls=0;
    assert(qjs_intl_native_provider_number_open(p,text("en"),text("latn"),&o,&n) == QJS_INTL_OK);
    count=h->calls;qjs_intl_native_number_close(n);n=NULL;assert(h->live==live);
    for(i=1;i<=count+1;i++) {
        h->calls=0;h->failure=i;n=(QJSIntlNativeNumber *)(uintptr_t)1;
        status=qjs_intl_native_provider_number_open(p,text("en"),text("latn"),&o,&n);
        h->failure=0;
        if(i<=count) assert(status==QJS_INTL_NO_MEMORY && !n);
        else {assert(status==QJS_INTL_OK && n);qjs_intl_native_number_close(n);}
        assert(h->live==live);
    }
    /* Repeated clearable zero output on failed locale/system/style admission. */
    n=(QJSIntlNativeNumber *)(uintptr_t)1;
    assert(qjs_intl_native_provider_number_open(p,text("zz"),text("latn"),&o,&n)==QJS_INTL_UNSUPPORTED && !n);
    n=(QJSIntlNativeNumber *)(uintptr_t)1;
    assert(qjs_intl_native_provider_number_open(p,text("en"),text("unknown"),&o,&n)==QJS_INTL_UNSUPPORTED && !n);
    qjs_intl_provider_free(p);
}
#endif
int main(void)
{
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
    Heap heap={0};QJSIntlAllocator a={&heap,allocate,resize,release};unsigned int i;
    provider_lifetime(&a);assert(!heap.live);
    for(i=0;i<3;i++){binary_snapshot(&a,i);assert(!heap.live);}
    installed_digest(&a);assert(!heap.live);
    named_compound_data(&a);assert(!heap.live);
    allocation_sweep(&a,&heap);assert(!heap.live);
#endif
    return 0;
}
