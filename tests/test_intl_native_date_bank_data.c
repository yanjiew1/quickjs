/* Root-owned C/OOM/lifetime integration gate over the one real binary owner. */
#include "../src/intl/date-native-data.h"
#include "../src/intl/data/date-data-validation.h"
#include "../src/intl/data/locale-metadata.h"
#include "../src/intl/metadata-data.h"
#include "../src/intl/provider-native-date.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static size_t calendar_mask_calls;
static QJSIntlStatus calendar_mask_status;
static int calendar_mask_zero;
static QJSIntlStatus counted_calendar_mask(const QJSIntlDataView *v,
                                           uint32_t locale, uint32_t *out)
{
    calendar_mask_calls++;
    assert(v && v->data == qjs_intl_locale_metadata_blob);
    if (calendar_mask_status) {
        *out = UINT32_MAX; /* Failed output must never become a ready cache. */
        return calendar_mask_status;
    }
    if (calendar_mask_zero) { *out = 0; return QJS_INTL_OK; }
    return qjs_intl_native_date_data_calendar_mask(v,locale,out);
}
/* As in test_timezone_system_reader, include the exact production owner with
 * one test-owned boundary replacement after its declarations. This translation
 * unit defines all provider symbols, so the archive provider object is not
 * extracted. Other date modules and the real data utility remain linked. */
#define qjs_intl_native_date_data_calendar_mask counted_calendar_mask
#include "../src/intl/provider-native.c"
#undef qjs_intl_native_date_data_calendar_mask

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
static QJSIntlBytes test_bytes(const char *s) { QJSIntlBytes b={s,strlen(s)}; return b; }
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
    o->requested.calendar=test_bytes("iso8601"); o->requested.time_zone=test_bytes("UTC");
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
static QJSIntlProvider *mask_provider(const QJSIntlAllocator *a)
{
    QJSIntlProviderConfig config={0}; QJSIntlProvider *p=NULL;
    config.backend=QJS_INTL_BACKEND_NATIVE; config.allocator=*a;
    config.default_locale=test_bytes("en-US"); config.default_time_zone=test_bytes("UTC");
    assert(!qjs_intl_provider_new(&config,&p) && p); return p;
}
static void calendar_masks(QJSIntlDataView *view, unsigned char *copy,
                            uint32_t locale, const QJSIntlAllocator *a, Probe *heap)
{
    QJSIntlProvider *first,*second,*empty;
    QJSIntlDataSection patterns; QJSIntlDataSlice row;
    QJSIntlDateBankOptions o; QJSIntlNativeDateProviderEnvironment state;
    QJSIntlNativeDateBank *bank=NULL;
    uint32_t expected,mask; unsigned char saved; size_t offset,attempts,live,i;
    static const char *const tags[]={"en","en-US"};
    /* The exported utility revalidates a caller-owned view even after success.
     * Restoring one malformed reserved pattern byte permits a real retry. */
    assert(!qjs_intl_native_date_data_calendar_mask(view,locale,&expected));
    assert(expected && !memcmp(copy,qjs_intl_locale_metadata_blob,qjs_intl_locale_metadata_blob_size));
    assert(!qjs_intl_data_section(view,100,&patterns) && patterns.record_count);
    assert(!qjs_intl_data_record(&patterns,0,&row) && row.length==32);
    offset=(size_t)(row.data-copy)+15; saved=copy[offset]; assert(!saved);
    copy[offset]=1; mask=UINT32_MAX;
    assert(qjs_intl_native_date_data_calendar_mask(view,locale,&mask)==QJS_INTL_DATA_ERROR && !mask);
    assert(copy[offset]==1);
    copy[offset]=saved;
    assert(!qjs_intl_native_date_data_calendar_mask(view,locale,&mask) && mask==expected);
    assert(!memcmp(copy,qjs_intl_locale_metadata_blob,qjs_intl_locale_metadata_blob_size));

    first=mask_provider(a); assert(!calendar_mask_calls);
    attempts=heap->attempts; live=heap->live; heap->fail=attempts+1;
    mask=UINT32_MAX;
    assert(qjs_intl_native_provider_date_calendar_mask(NULL,&mask)==QJS_INTL_INVALID_ARGUMENT && !mask);
    assert(qjs_intl_native_provider_date_calendar_mask(first,NULL)==QJS_INTL_INVALID_ARGUMENT);
    assert(!calendar_mask_calls);
    calendar_mask_status=QJS_INTL_DATA_ERROR; mask=UINT32_MAX;
    assert(qjs_intl_native_provider_date_calendar_mask(first,&mask)==QJS_INTL_DATA_ERROR && !mask);
    assert(calendar_mask_calls==1);
    calendar_mask_status=QJS_INTL_OK;
    assert(!qjs_intl_native_provider_date_calendar_mask(first,&mask) && mask==expected);
    assert(calendar_mask_calls==2);
    /* A ready entry must skip the validation boundary, even if it would fail.
     * Injecting a different status also verifies that failures were not cached. */
    calendar_mask_status=QJS_INTL_UNSUPPORTED;
    for(i=0;i<3;i++) {
        mask=UINT32_MAX;
        assert(!qjs_intl_native_provider_date_calendar_mask(first,&mask) && mask==expected);
    }
    assert(calendar_mask_calls==2 && heap->attempts==attempts && heap->live==live);
    heap->fail=0; calendar_mask_status=QJS_INTL_OK;
    /* Both installed locale spellings use the same proof in their durable
     * environment. Existing bank lifetime and exhaustive OOM cases follow. */
    options(&o,-1);
    for(i=0;i<sizeof(tags)/sizeof(tags[0]);i++) {
        assert(!qjs_intl_native_provider_date_open(first,test_bytes(tags[i]),test_bytes("latn"),
            &o,NULL,&state,&bank) && bank && state.calendar_mask==expected);
        qjs_intl_native_date_bank_close(bank); bank=NULL;
        assert(calendar_mask_calls==2 && heap->live==live);
    }
    second=mask_provider(a);
    assert(!qjs_intl_native_provider_date_calendar_mask(second,&mask) && mask==expected);
    assert(calendar_mask_calls==3); qjs_intl_provider_free(first);
    assert(!qjs_intl_native_provider_date_calendar_mask(second,&mask) && mask==expected);
    assert(calendar_mask_calls==3); qjs_intl_provider_free(second);
    /* Zero is a successful proof value, not a sentinel for an unready entry. */
    empty=mask_provider(a); calendar_mask_zero=1;
    assert(!qjs_intl_native_provider_date_calendar_mask(empty,&mask) && !mask);
    assert(calendar_mask_calls==4); calendar_mask_zero=0;
    assert(!qjs_intl_native_provider_date_calendar_mask(empty,&mask) && !mask);
    assert(calendar_mask_calls==4); qjs_intl_provider_free(empty);
    assert(!heap->live && !heap->failed);
    assert(!memcmp(copy,qjs_intl_locale_metadata_blob,qjs_intl_locale_metadata_blob_size));
}
static void tag_is(QJSIntlBytes tag, const char *expected)
{
    assert(tag.length==strlen(expected) && !memcmp(tag.data,expected,tag.length));
}
static void same_owned_tags(const QJSIntlTagList *a, const QJSIntlTagList *b)
{
    size_t i; assert(a->count==b->count && a->items!=b->items);
    for(i=0;i<a->count;i++) {
        assert(a->items[i].data!=b->items[i].data);
        assert(a->items[i].length==b->items[i].length);
        assert(!memcmp(a->items[i].data,b->items[i].data,a->items[i].length));
    }
}
static void inventory_order(QJSIntlProvider *p, const char *key, const QJSIntlTagList *list)
{
    const QJSIntlDataView *v=qjs_intl_native_provider_view(p);
    QJSIntlDataSection section; QJSIntlDataSlice tag,row;
    size_t at=1; uint32_t i,mask; unsigned int hc;
    static const char *const cycles[]={"h11","h12","h23","h24"};
    assert(list->count);
    if(!strcmp(key,"ca")) {
        tag_is(list->items[0],"gregory");
        assert(!qjs_intl_native_provider_date_calendar_mask(p,&mask));
        for(i=0;i<QJS_CAL_COUNT;i++) {
            const char *id=qjs_calendar_identifier((QJSCalendarId)i);
            if(!(mask&(UINT32_C(1)<<i)) || !qjs_calendar_is_supported((QJSCalendarId)i) ||
               !strcmp(id,"gregory")) continue;
            assert(at<list->count); tag_is(list->items[at++],id);
        }
    } else if(!strcmp(key,"hc")) {
        assert(!qjs_intl_native_provider_date_preferred_hour_cycle(p,test_bytes("en"),&hc));
        tag_is(list->items[0],cycles[hc]);
        for(i=0;i<4;i++) if(i!=hc) {
            assert(at<list->count); tag_is(list->items[at++],cycles[i]);
        }
    } else {
        tag_is(list->items[0],"latn");
        assert(!qjs_intl_data_section(v,QJS_INTL_DATA_NUMBERING,&section));
        for(i=0;i<section.record_count;i++) {
            assert(!qjs_intl_data_record(&section,i,&row));
            assert(!qjs_intl_data_record_string(v,&section,i,0,&tag));
            if(row.data[8]!=10 || row.data[9] ||
               (tag.length==4 && !memcmp(tag.data,"latn",4))) continue;
            assert(at<list->count && list->items[at].length==tag.length);
            assert(!memcmp(list->items[at++].data,tag.data,tag.length));
        }
    }
    assert(at==list->count);
}
static void successful_probe_cache(const unsigned char *source_copy)
{
    Probe heap={0},other_heap={0};
    QJSIntlAllocator a={&heap,allocate,resize,release};
    QJSIntlAllocator other_a={&other_heap,allocate,resize,release};
    QJSIntlProvider *p=mask_provider(&a),*other;
    QJSIntlTagList first={0},repeat={0};
    QJSIntlDateBankOptions o; QJSIntlNativeDateBank *bank=NULL;
    QJSIntlNativeDateProviderEnvironment state;
    size_t cold,warm,before,live,owners,owned_bytes,i,j,store_at,fail_at;
    uint32_t n=find(qjs_intl_native_provider_view(p),QJS_INTL_DATA_NUMBERING,"latn");
    static const char *const keys[]={"ca","hc","nu"};
    /* AvailableLocales warms exactly one complete capability proof. */
    heap.attempts=0;
    assert(!qjs_intl_native_provider_date_available(p,&first)); cold=heap.attempts;
    assert(first.count==2); tag_is(first.items[0],"en"); tag_is(first.items[1],"en-US");
    assert(p->date_probe_success_count==1);
    assert(qjs_intl_native_provider_date_probe_success(p,n,QJS_CAL_GREGORY,-1));
    heap.attempts=0;
    assert(!qjs_intl_native_provider_date_available(p,&repeat)); warm=heap.attempts;
    assert(warm==4 && cold>warm); same_owned_tags(&first,&repeat);
    qjs_intl_tag_list_clear(p,&first); qjs_intl_tag_list_clear(p,&repeat);
    /* A different provider must construct its own proof with its own allocator. */
    other=mask_provider(&other_a); other_heap.attempts=0;
    assert(!qjs_intl_native_provider_date_available(other,&first));
    assert(other_heap.attempts==cold && other->date_probe_success_count==1);
    qjs_intl_tag_list_clear(other,&first); qjs_intl_provider_free(other);
    assert(!other_heap.live);
    for(j=0;j<sizeof(keys)/sizeof(*keys);j++) {
        heap.attempts=0;
        assert(!qjs_intl_native_provider_date_key_values(p,test_bytes("en"),test_bytes(keys[j]),&first));
        before=heap.attempts; inventory_order(p,keys[j],&first);
        heap.attempts=0;
        assert(!qjs_intl_native_provider_date_key_values(p,test_bytes("en-US"),test_bytes(keys[j]),&repeat));
        warm=heap.attempts; assert(before>warm); same_owned_tags(&first,&repeat);
        qjs_intl_tag_list_clear(p,&repeat); live=heap.live;
        /* All warm output allocations still occur and every failure is visible.
         * Completed proofs can survive a later list-copy failure. */
        for(i=1;i<=warm;i++) {
            heap.attempts=0; heap.fail=i; heap.failed=0;
            assert(qjs_intl_native_provider_date_key_values(p,test_bytes("en-US"),test_bytes(keys[j]),&repeat)==QJS_INTL_NO_MEMORY);
            assert(heap.failed && !repeat.items && !repeat.count && heap.live==live);
            heap.attempts=0; heap.fail=0; heap.failed=0;
            assert(!qjs_intl_native_provider_date_key_values(p,test_bytes("en"),test_bytes(keys[j]),&repeat));
            assert(heap.attempts==warm); same_owned_tags(&first,&repeat);
            qjs_intl_tag_list_clear(p,&repeat); assert(heap.live==live);
        }
        qjs_intl_tag_list_clear(p,&first);
    }
    qjs_intl_native_provider_memory_usage(p,&owners,&owned_bytes);
    assert(owners==heap.live && owners==2+p->date_probe_success_count);
    assert(owned_bytes==sizeof(*p)+strlen(p->default_locale)+1+
        p->date_probe_success_count*sizeof(struct DateProbeSuccess));
    /* Locale rejection must precede reuse even with all inventories ready. */
    heap.attempts=0;
    assert(qjs_intl_native_provider_date_key_values(p,test_bytes("en-GB"),test_bytes("ca"),&repeat)==QJS_INTL_UNSUPPORTED);
    assert(!heap.attempts && !repeat.items && !repeat.count);
    /* Unsupported algorithmic rows remain rejected before allocation. */
    {
        QJSIntlDataSection section; QJSIntlDataSlice row,tag; int found=0;
        assert(!qjs_intl_data_section(qjs_intl_native_provider_view(p),QJS_INTL_DATA_NUMBERING,&section));
        for(i=0;i<section.record_count;i++) {
            assert(!qjs_intl_data_record(&section,(uint32_t)i,&row));
            if(row.data[8]==10 && !row.data[9]) continue;
            assert(!qjs_intl_data_record_string(qjs_intl_native_provider_view(p),&section,(uint32_t)i,0,&tag));
            options(&o,-1); heap.attempts=0; heap.fail=1;
            assert(qjs_intl_native_provider_date_open(p,test_bytes("en"),
                (QJSIntlBytes){(const char *)tag.data,tag.length},&o,NULL,&state,&bank)==QJS_INTL_UNSUPPORTED);
            assert(!bank && !heap.attempts && !heap.failed); found=1;
        }
        assert(found); heap.fail=0;
    }
    /* The public formatter path still allocates a fresh bank after warming. */
    options(&o,-1); live=heap.live; heap.attempts=0; heap.fail=1;
    assert(qjs_intl_native_provider_date_open(p,test_bytes("en-US"),test_bytes("latn"),
        &o,NULL,&state,&bank)==QJS_INTL_NO_MEMORY && !bank && heap.failed);
    assert(heap.live==live); heap.fail=0; heap.failed=0;
    assert(!qjs_intl_native_provider_date_open(p,test_bytes("en"),test_bytes("latn"),
        &o,NULL,&state,&bank) && bank);
    qjs_intl_native_date_bank_close(bank); assert(heap.live==live);
    qjs_intl_provider_free(p); assert(!heap.live);

    /* First bank allocation, cache storage, and each following list allocation
     * fail on fresh providers. Existing open_once sweeps all bank allocations. */
    memset(&heap,0,sizeof(heap)); p=mask_provider(&a);
    heap.attempts=0; calendar_mask_status=QJS_INTL_DATA_ERROR;
    assert(qjs_intl_native_provider_date_available(p,&repeat)==QJS_INTL_DATA_ERROR);
    assert(!heap.attempts && !p->date_probe_success_count && !repeat.items && !repeat.count);
    calendar_mask_status=QJS_INTL_OK;
    assert(!qjs_intl_native_provider_date_available(p,&repeat) && heap.attempts==cold);
    qjs_intl_tag_list_clear(p,&repeat); qjs_intl_provider_free(p); assert(!heap.live);
    store_at=cold-4;
    for(i=0;i<6;i++) {
        memset(&heap,0,sizeof(heap)); p=mask_provider(&a);
        fail_at=i ? store_at+i-1 : 1;
        heap.attempts=0; heap.fail=fail_at;
        assert(qjs_intl_native_provider_date_available(p,&repeat)==QJS_INTL_NO_MEMORY);
        assert(heap.failed && !repeat.items && !repeat.count);
        assert(p->date_probe_success_count==(fail_at>store_at ? 1u : 0u));
        assert(heap.live==2+p->date_probe_success_count);
        assert(qjs_intl_native_provider_date_probe_success(p,n,QJS_CAL_GREGORY,-1)==(fail_at>store_at));
        heap.attempts=0; heap.fail=0; heap.failed=0;
        assert(!qjs_intl_native_provider_date_available(p,&repeat));
        assert(heap.attempts==(fail_at>store_at ? 4u : cold));
        qjs_intl_tag_list_clear(p,&repeat); qjs_intl_provider_free(p); assert(!heap.live);
    }
    assert(!memcmp(source_copy,qjs_intl_locale_metadata_blob,qjs_intl_locale_metadata_blob_size));
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
static uint32_t read32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void write32(unsigned char *p, uint32_t value)
{
    unsigned int i; for(i=0;i<4;i++) p[i]=(unsigned char)(value>>(i*8));
}
static void zero_section(const QJSIntlDataSection *s)
{
    assert(!s->bytes.data && !s->bytes.length && !s->id &&
           !s->record_width && !s->record_count);
}
static void sorted_section_lookup(const QJSIntlDataView *view)
{
    static const uint32_t fields[] = {QJS_INTL_D_OFFSET, QJS_INTL_D_BYTE_LENGTH,
                                     QJS_INTL_D_RECORD_COUNT, QJS_INTL_D_RECORD_SIZE};
    QJSIntlDataSection s;
    QJSIntlDataView raw, rejected;
    uint32_t count = read32(view->data + QJS_INTL_H_SECTION_COUNT);
    size_t at = read32(view->data + QJS_INTL_H_DIRECTORY_OFFSET);
    uint32_t positions[3], misses[3], i, j;
    const unsigned char *first = view->data + at;
    const unsigned char *last;
    unsigned char *copy;
    assert(count >= 6 && view->length < UINT32_MAX);
    last = first + (size_t)(count - 1) * QJS_INTL_DATA_DIRECTORY_RECORD_SIZE;
    positions[0] = 0; positions[1] = count / 2; positions[2] = count - 1;
    misses[0] = read32(first + QJS_INTL_D_ID) - 1;
    misses[1] = 0;
    misses[2] = read32(last + QJS_INTL_D_ID) + 1;
    for (i = 1; i < count; i++) {
        const unsigned char *d = first + (size_t)i * QJS_INTL_DATA_DIRECTORY_RECORD_SIZE;
        uint32_t previous = read32(d - QJS_INTL_DATA_DIRECTORY_RECORD_SIZE);
        if (read32(d + QJS_INTL_D_ID) - previous > 1) { misses[1] = previous + 1; break; }
    }
    assert(misses[1] && misses[2]);
    for (i = 0; i < 3; i++) {
        const unsigned char *d = first + (size_t)positions[i] * QJS_INTL_DATA_DIRECTORY_RECORD_SIZE;
        memset(&s, 0xff, sizeof(s));
        assert(qjs_intl_data_section(view, read32(d + QJS_INTL_D_ID), &s) == QJS_INTL_DATA_OK);
        assert(s.id == read32(d + QJS_INTL_D_ID) && s.bytes.data == view->data + read32(d + QJS_INTL_D_OFFSET));
        assert(s.bytes.length == read32(d + QJS_INTL_D_BYTE_LENGTH) &&
               s.record_width == read32(d + QJS_INTL_D_RECORD_SIZE) &&
               s.record_count == read32(d + QJS_INTL_D_RECORD_COUNT));
        memset(&s, 0xff, sizeof(s));
        assert(qjs_intl_data_section(view, misses[i], &s) == QJS_INTL_DATA_NOT_FOUND);
        zero_section(&s);
    }
    memset(&s, 0xff, sizeof(s));
    assert(qjs_intl_data_section(NULL, 1, &s) == QJS_INTL_DATA_INVALID_ARGUMENT);
    zero_section(&s);
    assert(qjs_intl_data_section(view, 1, NULL) == QJS_INTL_DATA_INVALID_ARGUMENT);
    raw = *view; raw.length = QJS_INTL_DATA_HEADER_SIZE - 1;
    memset(&s, 0xff, sizeof(s));
    assert(qjs_intl_data_section(&raw, 1, &s) == QJS_INTL_DATA_INVALID);
    zero_section(&s);
    /* Malformed raw copies probe retained defensive bounds; the successfully
     * opened source view and its immutable blob remain unchanged. */
    copy = malloc(view->length); assert(copy);
    raw.data = copy; raw.length = view->length;
    for (i = 0; i < 3; i++) for (j = 0; j < sizeof(fields) / sizeof(*fields); j++) {
        unsigned char *d = copy + at + (size_t)positions[i] * QJS_INTL_DATA_DIRECTORY_RECORD_SIZE;
        memcpy(copy, view->data, view->length);
        write32(d + fields[j], fields[j] == QJS_INTL_D_RECORD_SIZE ? 0 : UINT32_MAX);
        memset(&s, 0xff, sizeof(s));
        assert(qjs_intl_data_section(&raw, read32(d + QJS_INTL_D_ID), &s) == QJS_INTL_DATA_INVALID);
        zero_section(&s);
    }
    memcpy(copy, view->data, view->length);
    write32(copy + QJS_INTL_H_SECTION_COUNT, UINT32_MAX);
    memset(&s, 0xff, sizeof(s));
    assert(qjs_intl_data_section(&raw, 1, &s) == QJS_INTL_DATA_INVALID);
    zero_section(&s);
    /* Corrupt the final ID, after any List40 lookup made during layout checks.
     * Duplicate and descending directory IDs must still fail open. */
    for (i = 0; i < 2; i++) {
        unsigned char *d = copy + at + (size_t)(count - 1) * QJS_INTL_DATA_DIRECTORY_RECORD_SIZE;
        memcpy(copy, view->data, view->length);
        write32(d + QJS_INTL_D_ID, read32(d - QJS_INTL_DATA_DIRECTORY_RECORD_SIZE) - i);
        memset(&rejected, 0xff, sizeof(rejected));
        assert(qjs_intl_data_open(copy, view->length, &rejected) == QJS_INTL_DATA_INVALID);
        assert(!rejected.data && !rejected.length);
    }
    free(copy);
}
static void write64(unsigned char *p, int64_t value)
{
    uint64_t bits=(uint64_t)value; unsigned int i;
    for(i=0;i<8;i++) p[i]=(unsigned char)(bits>>(i*8));
}
static unsigned char *directory(unsigned char *blob, uint32_t id)
{
    uint32_t i,count=read32(blob+QJS_INTL_H_SECTION_COUNT);
    unsigned char *d=blob+read32(blob+QJS_INTL_H_DIRECTORY_OFFSET);
    for(i=0;i<count;i++,d+=QJS_INTL_DATA_DIRECTORY_RECORD_SIZE)
        if(read32(d+QJS_INTL_D_ID)==id) return d;
    abort();
}
static void remove_section(unsigned char *blob, uint32_t id)
{
    unsigned char *d=directory(blob,id);
    uint32_t count=read32(blob+QJS_INTL_H_SECTION_COUNT);
    unsigned char *end=blob+read32(blob+QJS_INTL_H_DIRECTORY_OFFSET)+
        count*QJS_INTL_DATA_DIRECTORY_RECORD_SIZE;
    memset(blob+read32(d+QJS_INTL_D_OFFSET),0,read32(d+QJS_INTL_D_BYTE_LENGTH));
    memmove(d,d+QJS_INTL_DATA_DIRECTORY_RECORD_SIZE,(size_t)(end-d)-QJS_INTL_DATA_DIRECTORY_RECORD_SIZE);
    memset(end-QJS_INTL_DATA_DIRECTORY_RECORD_SIZE,0,QJS_INTL_DATA_DIRECTORY_RECORD_SIZE);
    write32(blob+QJS_INTL_H_SECTION_COUNT,count-1);
}
static void rejected(const unsigned char *blob, size_t length)
{
    QJSIntlDataView view={(const unsigned char *)1,1};
    assert(qjs_intl_data_open(blob,length,&view)==QJS_INTL_DATA_INVALID);
    assert(!view.data && !view.length);
}
static QJSIntlStatus name_zone(void *opaque, QJSIntlBytes zone, int64_t seconds,
                              QJSIntlDateZoneInfo *out)
{
    (void)zone; (void)seconds; *out=*(QJSIntlDateZoneInfo *)opaque; return QJS_INTL_OK;
}
static void zone_part(const QJSIntlFormatted *r, const char *expected)
{
    size_t i,j; int found=0;
    for(i=0;i<r->part_count;i++) if(r->parts[i].type==QJS_INTL_PART_TIME_ZONE_NAME) {
        const QJSIntlPart *part=&r->parts[i]; assert(!found);
        assert(part->end-part->start==strlen(expected));
        for(j=0;j<strlen(expected);j++) assert(r->text[part->start+j]==(unsigned char)expected[j]);
        found=1;
    }
    assert(found);
}
static void name_offset_join(void)
{
    /* Independent CLDR witnesses, including Lisbon's two adjacent metazones.
       Deliberate signed offsets make the overrides disagree with TZif flags. */
    static const struct {
        const char *zone; int64_t epoch; int32_t standard,daylight;
        const char *names[2];
    } cases[]={
        {"America/Denver",INT64_C(2208988800000),-12345,12345,
            {"Mountain Standard Time","Mountain Daylight Time"}},
        {"Europe/Dublin",INT64_C(2208988800000),1800,-1800,
            {"Greenwich Mean Time (Ireland)","Irish Standard Time"}},
        {"Europe/Lisbon",INT64_C(717555599999),-23456,23456,
            {"Western European Standard Time (Lisbon)","Western European Summer Time (Lisbon)"}},
        {"Europe/Lisbon",INT64_C(717555600000),3456,-3456,
            {"Central European Standard Time (Lisbon)","Central European Summer Time (Lisbon)"}},
        {"Pacific/Chatham",INT64_C(2208988800000),-4567,4567,
            {"Chatham Standard Time","Chatham Daylight Time"}}
    };
    static const char *const tags[]={"en","en-US"};
    enum { CASES=sizeof(cases)/sizeof(cases[0]), LOCALES=sizeof(tags)/sizeof(tags[0]) };
    QJSIntlDataView original,view;
    QJSIntlDataSection periods,offsets;
    QJSIntlNativeDate *handles[LOCALES][CASES];
    QJSIntlDateZoneInfo zone={0}; QJSIntlDateEnvironment environment={0};
    Probe p={0}; QJSIntlAllocator a={&p,allocate,resize,release};
    size_t old_size=qjs_intl_locale_metadata_blob_size,at=(old_size+3)&~(size_t)3;
    size_t length=at+CASES*QJS_INTL_DATE_NAME_OFFSETS_WIDTH;
    unsigned char *blob=calloc(1,length),*bad=malloc(length),*d,*rows;
    uint32_t numbering,i,j,l,owners[CASES]; int selected;
    assert(blob && bad && length<=UINT32_MAX);
    assert(!qjs_intl_data_open(qjs_intl_locale_metadata_blob,old_size,&original));
    assert(!qjs_intl_data_section(&original,QJS_INTL_DATA_DATE_META_PERIOD,&periods));
    assert(!qjs_intl_data_section(&original,QJS_INTL_DATA_DATE_NAME_OFFSETS,&offsets));
    memcpy(blob,original.data,old_size);
    /* Relocate only109. Its old payload becomes zero padding; all other bytes
       retain the real binary owner's locale/name/period records. */
    memset(blob+(offsets.bytes.data-original.data),0,offsets.bytes.length);
    write32(blob+QJS_INTL_H_TOTAL_SIZE,(uint32_t)length);
    d=directory(blob,QJS_INTL_DATA_DATE_NAME_OFFSETS);
    write32(d+QJS_INTL_D_OFFSET,(uint32_t)at);
    write32(d+QJS_INTL_D_BYTE_LENGTH,CASES*QJS_INTL_DATE_NAME_OFFSETS_WIDTH);
    write32(d+QJS_INTL_D_RECORD_COUNT,CASES); rows=blob+at;
    for(i=0;i<CASES;i++) {
        QJSIntlDataSlice key,row; int64_t from,before;
        for(j=0;j<periods.record_count;j++) {
            assert(!qjs_intl_data_record_string(&original,&periods,j,0,&key));
            assert(!qjs_intl_date_data_i64(&periods,j,16,&from));
            assert(!qjs_intl_date_data_i64(&periods,j,24,&before));
            if(key.length==strlen(cases[i].zone) && !memcmp(key.data,cases[i].zone,key.length) &&
               cases[i].epoch>=from && cases[i].epoch<before) break;
        }
        assert(j<periods.record_count); owners[i]=j;
        if(i) assert(owners[i]>owners[i-1]); /* several skipped105 rows, then a new zone */
        assert(!qjs_intl_data_record(&periods,j,&row));
        memcpy(rows+i*32,row.data,8); memcpy(rows+i*32+8,row.data+16,16);
        write32(rows+i*32+24,(uint32_t)cases[i].standard);
        write32(rows+i*32+28,(uint32_t)cases[i].daylight);
    }
    assert(!qjs_intl_data_open(blob,length,&view));
    assert(!qjs_intl_date_data_validate(&view));
    numbering=find(&view,QJS_INTL_DATA_NUMBERING,"latn");
    /* Each invalid input must fail before a decoder receives a view. */
    memcpy(bad,blob,length); memcpy(bad+at+32,bad+at,32); rejected(bad,length); /* duplicate */
    memcpy(bad,blob,length);
    memcpy(bad+at,rows+(CASES-1)*32,32); memcpy(bad+at+(CASES-1)*32,rows,32);
    rejected(bad,length); /* unsorted */
    memcpy(bad,blob,length);
    {
        int64_t from; assert(!qjs_intl_date_data_i64(&periods,owners[0],16,&from));
        assert(from<cases[0].epoch); write64(bad+at+8,from+1);
    }
    rejected(bad,length); /* ordered override with no exact owning period */
    memcpy(bad,blob,length); remove_section(bad,QJS_INTL_DATA_DATE_META_PERIOD);
    rejected(bad,length); /* missing105 */
    memcpy(bad,blob,length); d=directory(bad,QJS_INTL_DATA_DATE_NAME_OFFSETS);
    memset(bad+at,0,CASES*32);
    write32(d+QJS_INTL_D_BYTE_LENGTH,0); write32(d+QJS_INTL_D_RECORD_COUNT,0);
    {
        QJSIntlDataView empty; assert(!qjs_intl_data_open(bad,length,&empty));
        assert(!qjs_intl_date_data_validate(&empty)); /* empty overrides are valid */
    }
    memcpy(bad,blob,length); remove_section(bad,QJS_INTL_DATA_DATE_NAME_OFFSETS);
    {
        QJSIntlDataView absent; assert(!qjs_intl_data_open(bad,length,&absent));
        assert(!qjs_intl_date_data_validate(&absent)); /* overrides are optional */
    }
    free(bad); environment.opaque=&zone; environment.zone=name_zone;
    for(l=0;l<LOCALES;l++) for(i=0;i<CASES;i++) {
        QJSIntlDateBankOptions o; options(&o,-1);
        for(j=0;j<QJS_DATE_FIELD_COUNT;j++) o.requested.fields[j]=-1;
        o.requested.fields[QJS_DATE_HOUR]=QJS_DATE_NUMERIC;
        o.requested.fields[QJS_DATE_ZONE]=QJS_DATE_ZONE_LONG;
        o.requested.time_zone=test_bytes(cases[i].zone);
        assert(!qjs_intl_native_date_open_data(&a,&view,find(&view,QJS_INTL_DATA_LOCALE,tags[l]),
            numbering,&o.requested,&environment,&handles[l][i]));
    }
    memset(blob,0x5a,length); free(blob); /* decoded names and offsets must be owned */
    for(l=0;l<LOCALES;l++) for(i=0;i<CASES;i++) {
        for(selected=0;selected<2;selected++) {
            QJSIntlFormatted result={0};
            zone.offset_seconds=selected ? cases[i].daylight : cases[i].standard;
            zone.daylight=!selected;
            assert(!qjs_intl_native_date_format(handles[l][i],cases[i].epoch,&result));
            zone_part(&result,cases[i].names[selected]); qjs_intl_native_date_clear(&a,&result);
        }
        qjs_intl_native_date_close(handles[l][i]);
    }
    assert(!p.live);
}
/* UTF8 scalar boundaries and failed-decoding output preservation. */
static void date_utf8_boundaries(void)
{
    static const unsigned char valid[] = {
        0x00,0x7f,0xc2,0x80,0xdf,0xbf,0xe0,0xa0,0x80,
        0xed,0x9f,0xbf,0xee,0x80,0x80,0xef,0xbf,0xbf,
        0xf0,0x90,0x80,0x80,0xf4,0x8f,0xbf,0xbf
    };
    static const uint32_t scalars[] = {
        0,0x7f,0x80,0x7ff,0x800,0xd7ff,0xe000,0xffff,0x10000,0x10ffff
    };
    static const size_t ends[] = {1,2,4,6,9,12,15,18,22,26};
    static const struct { unsigned char bytes[4]; size_t length; } bad[] = {
        {{0x80},1}, {{0xc0,0x80},2}, {{0xc2},1}, {{0xc2,0x7f},2},
        {{0xe0,0x9f,0xbf},3}, {{0xe0,0xa0},2}, {{0xed,0xa0,0x80},3},
        {{0xf0,0x8f,0xbf,0xbf},4}, {{0xf0,0x90,0x80},3},
        {{0xf4,0x90,0x80,0x80},4}, {{0xf5,0x80,0x80,0x80},4}
    };
    QJSIntlBytes text={(const char *)valid,sizeof(valid)};
    size_t at=0,i; uint32_t cp=UINT32_MAX;
    for(i=0;i<sizeof(scalars)/sizeof(*scalars);i++) {
        assert(qjs_intl_date_utf8_next(text,&at,&cp)==QJS_INTL_OK);
        assert(at==ends[i] && cp==scalars[i]);
    }
    cp=UINT32_MAX;
    assert(qjs_intl_date_utf8_next(text,&at,&cp)==QJS_INTL_INVALID_ARGUMENT);
    assert(at==sizeof(valid) && cp==UINT32_MAX);
    for(i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        unsigned char bytes[5]={0x41};
        memcpy(bytes+1,bad[i].bytes,bad[i].length);
        text.data=(const char *)bytes; text.length=bad[i].length+1;
        at=1; cp=UINT32_MAX;
        assert(qjs_intl_date_utf8_next(text,&at,&cp)==QJS_INTL_DATA_ERROR);
        assert(at==1 && cp==UINT32_MAX);
    }
}


static void partial_period_names(void)
{
    static const QJSIntlDatePattern pattern = {
        QJS_DATE_AVAILABLE, 255, 1, {"Bh", 2}, {"h B", 3}
    };
    static const QJSIntlDateName name = {
        QJS_DATE_NAME_PERIOD, 0, 1, QJS_DATE_MORNING1, {"morning", 7}
    };
    static const QJSIntlDatePeriodRule rule = { QJS_DATE_MORNING1, 0, 0, 86400 };
    QJSIntlDateData data = {0};
    QJSIntlDateOptions options = {0};
    QJSIntlNativeDate *date;
    QJSIntlFormatted result = {0};
    Probe probe = {0};
    QJSIntlAllocator allocator = {&probe, allocate, resize, release};
    int fields[QJS_DATE_FIELD_COUNT];
    size_t i;
    data.patterns = &pattern; data.pattern_count = 1;
    data.names = &name; data.name_count = 1;
    data.periods = &rule; data.period_count = 1;
    for (i = 0; i < 10; i++) data.digits[i] = '0' + i;
    options.calendar = test_bytes("gregory"); options.time_zone = test_bytes("UTC");
    options.hour_cycle = QJS_DATE_H12;
    options.date_style = options.time_style = -1;
    for (i = 0; i < QJS_DATE_FIELD_COUNT; i++) options.fields[i] = -1;
    options.fields[QJS_DATE_DAY_PERIOD] = QJS_DATE_LONG;
    for (options.format_matcher = QJS_DATE_BASIC;
         options.format_matcher <= QJS_DATE_BEST_FIT; options.format_matcher++) {
        assert(!qjs_intl_native_date_open(&allocator, &data, &options, NULL, &date));
        qjs_intl_native_date_resolved_fields(date, fields);
        assert(fields[QJS_DATE_DAY_PERIOD] == QJS_DATE_SHORT);
        assert(fields[QJS_DATE_HOUR] == -1);
        assert(!qjs_intl_native_date_format(date, 0, &result));
        assert(result.length == 7 && result.part_count == 1);
        for (i = 0; i < result.length; i++) assert(result.text[i] == (unsigned char)"morning"[i]);
        assert(result.parts[0].type == QJS_INTL_PART_DAY_PERIOD);
        qjs_intl_native_date_clear(&allocator, &result);
        qjs_intl_native_date_close(date);
        assert(!probe.live);
    }
}

int main(void)
{
    QJSIntlDataView view; QJSIntlDateBankOptions o;
    QJSIntlNativeDateBank *bank; QJSIntlDateValue value={0},other={0};
    QJSIntlFormatted result={0}; QJSIntlDateValueFault fault;
    Probe p={0}; QJSIntlAllocator a={&p,allocate,resize,release};
    unsigned char *copy=malloc(qjs_intl_locale_metadata_blob_size);
    uint32_t locale,numbering; size_t count,i,baseline; int style;
    date_utf8_boundaries();
    partial_period_names();
    name_offset_join();
    assert(copy); memcpy(copy,qjs_intl_locale_metadata_blob,qjs_intl_locale_metadata_blob_size);
    assert(!qjs_intl_data_open(copy,qjs_intl_locale_metadata_blob_size,&view));
    sorted_section_lookup(&view);
    locale=find(&view,QJS_INTL_DATA_LOCALE,"en"); numbering=find(&view,QJS_INTL_DATA_NUMBERING,"latn");
    calendar_masks(&view,copy,locale,&a,&p);
    successful_probe_cache(copy);
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
