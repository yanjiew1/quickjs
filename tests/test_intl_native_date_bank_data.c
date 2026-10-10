/* Root-owned C/OOM/lifetime integration gate over the one real binary owner. */
#include "intl/date-native-data.h"
#include "intl/data/locale-metadata.h"
#include "intl/metadata-data.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef struct Probe { size_t live, attempts, fail; int failed; } Probe;
static int fail(Probe *p) { p->attempts++; if (p->attempts == p->fail) { p->failed=1; return 1; } return 0; }
static void *allocate(void *opaque, size_t size)
{
    Probe *p=opaque; void *value; if (fail(p)) return NULL;
    value=malloc(size); if (value) p->live++; return value;
}
static void *resize(void *opaque, void *value, size_t size)
{
    Probe *p=opaque; void *result; int empty=value==NULL;
    assert(size); if (fail(p)) return NULL;
    result=realloc(value,size); if (result && empty) p->live++; return result;
}
static void release(void *opaque, void *value)
{
    Probe *p=opaque; if (value) { assert(p->live); p->live--; free(value); }
}
static QJSIntlBytes bytes(const char *s) { QJSIntlBytes b={s,strlen(s)}; return b; }
static uint32_t find(const QJSIntlDataView *v, uint32_t id, const char *name)
{
    QJSIntlDataSection s; QJSIntlDataSlice tag; uint32_t i;
    assert(!qjs_intl_data_section(v,id,&s));
    for(i=0;i<s.record_count;i++) {
        assert(!qjs_intl_data_record_string(v,&s,i,0,&tag));
        if(tag.length==strlen(name) && !memcmp(tag.data,name,tag.length)) return i;
    }
    abort();
}
static void options(QJSIntlDateBankOptions *o, int style)
{
    size_t i; memset(o,0,sizeof(*o));
    o->requested.calendar=bytes("iso8601"); o->requested.time_zone=bytes("UTC");
    o->requested.hour_cycle=QJS_DATE_H23; o->requested.format_matcher=QJS_DATE_BEST_FIT;
    o->requested.date_style=o->requested.time_style=style;
    for(i=0;i<QJS_DATE_FIELD_COUNT;i++) o->requested.fields[i]=-1;
    if(style<0) {
        o->requested.fields[QJS_DATE_HOUR]=QJS_DATE_TWO_DIGIT;
        o->requested.fields[QJS_DATE_MINUTE]=QJS_DATE_TWO_DIGIT;
        o->requested.fields[QJS_DATE_SECOND]=QJS_DATE_TWO_DIGIT;
        o->requested.fields[QJS_DATE_FRACTION]=3;
    }
    o->number_required=QJS_DATE_REQUIRE_ANY; o->number_defaults=QJS_DATE_DEFAULT_DATE;
    o->instant_defaults=QJS_DATE_DEFAULT_ALL;
}
static size_t open_once(const QJSIntlDataView *v, uint32_t l, uint32_t n, int style, size_t failure)
{
    Probe p={0}; QJSIntlAllocator a={&p,allocate,resize,release};
    QJSIntlDateBankOptions o; QJSIntlNativeDateBank *bank=(void *)1;
    QJSIntlStatus status; options(&o,style); p.fail=failure;
    status=qjs_intl_native_date_bank_open_data(&a,v,l,n,&o,NULL,&bank);
    if(status) { assert(status==QJS_INTL_NO_MEMORY && p.failed && !bank); }
    else { assert(bank && !p.failed); qjs_intl_native_date_bank_close(bank); }
    assert(!p.live); return p.attempts;
}
static void fraction(const QJSIntlFormatted *r, const char *expected)
{
    size_t i,j; int found=0;
    for(i=0;i<r->part_count;i++) if(r->parts[i].type==QJS_INTL_PART_FRACTIONAL_SECOND) {
        const QJSIntlPart *part=&r->parts[i]; assert(part->end-part->start==strlen(expected));
        for(j=0;j<strlen(expected);j++) assert(r->text[part->start+j]==(unsigned char)expected[j]);
        found=1;
    }
    assert(found);
}
int main(void)
{
    QJSIntlDataView view; QJSIntlDateBankOptions o;
    QJSIntlNativeDateBank *bank; QJSIntlDateValue value={0},other={0};
    QJSIntlFormatted result={0}; QJSIntlDateValueFault fault;
    Probe p={0}; QJSIntlAllocator a={&p,allocate,resize,release};
    unsigned char *copy=malloc(qjs_intl_locale_metadata_blob_size);
    uint32_t locale,numbering; size_t count,i,baseline; int style;
    assert(copy); memcpy(copy,qjs_intl_locale_metadata_blob,qjs_intl_locale_metadata_blob_size);
    assert(!qjs_intl_data_open(copy,qjs_intl_locale_metadata_blob_size,&view));
    locale=find(&view,QJS_INTL_DATA_LOCALE,"en"); numbering=find(&view,QJS_INTL_DATA_NUMBERING,"latn");
    for(style=-1;style<=QJS_DATE_STYLE_SHORT;style++) {
        count=open_once(&view,locale,numbering,style,SIZE_MAX); assert(count);
        for(i=1;i<=count+1;i++) open_once(&view,locale,numbering,style,i);
    }
    options(&o,-1); assert(!qjs_intl_native_date_bank_open_data(&a,&view,locale,numbering,&o,NULL,&bank));
    /* Destroy the entire source view: every bank slot must own its snapshot. */
    memset(copy,0x5a,qjs_intl_locale_metadata_blob_size); free(copy); memset(&view,0,sizeof(view));
    value.kind=QJS_DATE_VALUE_INSTANT; value.value.instant=qjs_temporal_epoch_ns_from_int64(-1);
    assert(!qjs_intl_native_date_bank_format(bank,&value,&result,&fault)); fraction(&result,"999");
    qjs_intl_native_date_clear(&a,&result); baseline=p.live;
    p.attempts=0; p.fail=SIZE_MAX;
    assert(!qjs_intl_native_date_bank_format(bank,&value,&result,&fault)); count=p.attempts;
    qjs_intl_native_date_clear(&a,&result); assert(p.live==baseline);
    for(i=1;i<=count+1;i++) {
        QJSIntlStatus status; p.attempts=0; p.fail=i; p.failed=0;
        status=qjs_intl_native_date_bank_format(bank,&value,&result,&fault);
        assert(status==QJS_INTL_OK || (status==QJS_INTL_NO_MEMORY && p.failed));
        if(status) assert(!result.text && !result.parts && !result.length && !result.part_count);
        qjs_intl_native_date_clear(&a,&result); assert(p.live==baseline);
        p.fail=0; p.failed=0;
        assert(!qjs_intl_native_date_bank_format(bank,&value,&result,&fault));
        qjs_intl_native_date_clear(&a,&result); assert(p.live==baseline);
    }
    p.fail=0; other=value; other.value.instant=qjs_temporal_epoch_ns_from_int64(0);
    assert(!qjs_intl_native_date_bank_range(bank,&value,&other,&result,&fault));
    { int start=0,end=0; for(i=0;i<result.part_count;i++) {
        start|=result.parts[i].source==QJS_INTL_SOURCE_START_RANGE;
        end|=result.parts[i].source==QJS_INTL_SOURCE_END_RANGE;
    } assert(start && end); }
    qjs_intl_native_date_clear(&a,&result);
    assert(!qjs_intl_native_date_bank_range(bank,&other,&value,&result,&fault));
    qjs_intl_native_date_clear(&a,&result);
    other.kind=QJS_DATE_VALUE_PLAIN_DATE;
    assert(qjs_intl_native_date_bank_range(bank,&value,&other,&result,&fault)==QJS_INTL_INVALID_ARGUMENT);
    assert(fault==QJS_DATE_FAULT_MIXED_TYPE && !result.text && !result.parts);
    qjs_intl_native_date_bank_close(bank); assert(!p.live); return 0;
}
