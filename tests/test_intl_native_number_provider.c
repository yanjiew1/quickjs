/* Native Number handle/provider lifetime, binary snapshot and allocation gates.
 * Copyright (c) 2026 Yan-Jie Wang. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "../src/intl/provider-native-number.h"
#include "../src/intl/data/locale-metadata.h"
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
    assert(qjs_intl_native_provider_number_open(p,text("fr"),text("latn"),&o,&n)==QJS_INTL_UNSUPPORTED && !n);
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
    allocation_sweep(&a,&heap);assert(!heap.live);
#endif
    return 0;
}
