/* NumberFormat data adapter, source-only development profile en/en-US.
 * Copyright (c) 2026 Yan-Jie Wang. No global state, ICU or second blob owner. */
#include "provider-native-number.h"
#include <string.h>
static int same(QJSIntlBytes a, const char *s)
{
    size_t n = strlen(s);
    return a.data && a.length == n && !memcmp(a.data, s, n);
}
static QJSIntlStatus named_index(const QJSIntlDataView *v, uint32_t id,
                                 QJSIntlBytes name, uint32_t *out)
{
    QJSIntlDataSection s;
    QJSIntlDataSlice tag;
    uint32_t i;
    QJSIntlDataStatus status = qjs_intl_data_section(v, id, &s);
    if (status == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (status != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < s.record_count; i++) {
        if (qjs_intl_data_record_string(v, &s, i, 0, &tag) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        if (tag.length == name.length && !memcmp(tag.data, name.data, name.length)) {
            *out = i; return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}
QJSIntlStatus qjs_intl_native_provider_number_open(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlBytes numbering, const QJSIntlNumberOptions *o,
    QJSIntlNativeNumber **out)
{
    const QJSIntlDataView *v;
    QJSIntlStatus status;
    uint32_t li, ni;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!p || !o || !numbering.data || !numbering.length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!same(locale, "en") && !same(locale, "en-US")) return QJS_INTL_UNSUPPORTED;
    v = qjs_intl_native_provider_view(p);
    if (!v) return QJS_INTL_INVALID_ARGUMENT;
    status = named_index(v, QJS_INTL_DATA_LOCALE, (QJSIntlBytes){"en",2}, &li);
    if (status != QJS_INTL_OK) return status;
    status = named_index(v, QJS_INTL_DATA_NUMBERING, numbering, &ni);
    if (status != QJS_INTL_OK) return status;
    return qjs_intl_native_number_open_data(qjs_intl_native_provider_allocator(p), v, li, ni, o, out);
}
QJSIntlStatus qjs_intl_native_provider_number_currency_digits(QJSIntlProvider *p,
    QJSIntlBytes currency, unsigned int *out)
{
    if (!p) { if (out) *out = 0; return QJS_INTL_INVALID_ARGUMENT; }
    return qjs_intl_native_number_currency_digits(qjs_intl_native_provider_view(p), currency, out);
}
static void default_options(QJSIntlNumberOptions *o)
{
    memset(o, 0, sizeof(*o));
    o->grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    o->digits.minimum_integer_digits = 1;
    o->digits.maximum_fraction_digits = 3;
    o->digits.minimum_significant_digits = 1;
    o->digits.maximum_significant_digits = 21;
    o->digits.rounding_increment = 1;
    o->digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    o->digits.maximum_output_length = o->maximum_output_length = 1048576;
}
QJSIntlStatus qjs_intl_native_provider_number_available(QJSIntlProvider *p)
{
    QJSIntlNumberOptions o;
    QJSIntlNativeNumber *number = NULL;
    QJSIntlStatus status;
    default_options(&o);
    status = qjs_intl_native_provider_number_open(p, (QJSIntlBytes){"en",2},
        (QJSIntlBytes){"latn",4}, &o, &number);
    qjs_intl_native_number_close(number);
    return status;
}
/* Reader-validated sorted keys admit numeric systems without constructing
 * and discarding a complete snapshot per entry. Final formatter open still
 * proves the selected style and all its required optional rows. */
static int decimal_rows(const QJSIntlDataView *v, uint32_t locale, uint32_t numbering)
{
    const uint32_t ids[] = {QJS_INTL_DATA_NUMBER_SYMBOL,QJS_INTL_DATA_NUMBER_PATTERN};
    size_t pass;
    for(pass=0;pass<2;pass++) {
        QJSIntlDataSection s;
        QJSIntlDataSlice row;
        uint32_t low=0,high,a,b;
        int found=0;
        QJSIntlDataStatus status=qjs_intl_data_section(v,ids[pass],&s);
        if(status==QJS_INTL_DATA_NOT_FOUND) return 0;
        if(status!=QJS_INTL_DATA_OK) return -1;
        high=s.record_count;
        while(low<high) {
            uint32_t middle=low+(high-low)/2;
            if(qjs_intl_data_record_u32(&s,middle,0,&a)!=QJS_INTL_DATA_OK ||
               qjs_intl_data_record_u32(&s,middle,4,&b)!=QJS_INTL_DATA_OK) return -1;
            if(a<locale || (a==locale && b<numbering)) low=middle+1; else high=middle;
        }
        while(low<s.record_count) {
            if(qjs_intl_data_record_u32(&s,low,0,&a)!=QJS_INTL_DATA_OK ||
               qjs_intl_data_record_u32(&s,low,4,&b)!=QJS_INTL_DATA_OK ||
               qjs_intl_data_record(&s,low,&row)!=QJS_INTL_DATA_OK) return -1;
            if(a!=locale || b!=numbering) break;
            if(!pass || (row.data[8]==0 && row.data[9]==0)) {found=1;break;}
            low++;
        }
        if(!found) return 0;
    }
    return 1;
}

/* Admit only numbering systems whose actual en decimal rows are validated.
 * Default latn is explicit for this en profile, first in LocaleData. Numeric
 * systems with missing source rows and algorithmic systems are excluded.
 * Results own separate NUL-terminated strings; no binary slices escape. */
QJSIntlStatus qjs_intl_native_provider_number_key_values(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlBytes key, QJSIntlTagList *out)
{
    const QJSIntlDataView *v;
    const QJSIntlAllocator *a;
    QJSIntlDataSection s;
    QJSIntlDataSlice tag;
    QJSIntlStatus status;
    uint32_t i, en;
    size_t pass, n;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!p) return QJS_INTL_INVALID_ARGUMENT;
    if ((!same(locale,"en") && !same(locale,"en-US")) || !same(key,"nu"))
        return QJS_INTL_UNSUPPORTED;
    v = qjs_intl_native_provider_view(p); a = qjs_intl_native_provider_allocator(p);
    if (qjs_intl_data_section(v, QJS_INTL_DATA_NUMBERING, &s) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    n = s.record_count;
    if (!n) return QJS_INTL_UNSUPPORTED;
    if (n > SIZE_MAX / sizeof(*out->items)) return QJS_INTL_OVERFLOW;
    out->items = a->malloc(a->opaque, n * sizeof(*out->items));
    if (!out->items) return QJS_INTL_NO_MEMORY;
    memset(out->items, 0, n * sizeof(*out->items));
    status=named_index(v,QJS_INTL_DATA_LOCALE,(QJSIntlBytes){"en",2},&en);
    if(status!=QJS_INTL_OK) goto fail;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < s.record_count; i++) {
            QJSIntlBytes candidate;
            char *copy;
            QJSIntlDataSlice system;
            int admitted;
            if (qjs_intl_data_record_string(v, &s, i, 0, &tag) != QJS_INTL_DATA_OK) {
                status = QJS_INTL_DATA_ERROR; goto fail;
            }
            candidate = (QJSIntlBytes){(const char *)tag.data, tag.length};
            if (same(candidate,"latn") != (pass == 0)) continue;
            if(qjs_intl_data_record(&s,i,&system)!=QJS_INTL_DATA_OK) {
                status=QJS_INTL_DATA_ERROR;goto fail;
            }
            if(system.data[8]!=10 || system.data[9]) continue;
            admitted=decimal_rows(v,en,i);
            if(admitted<0) {status=QJS_INTL_DATA_ERROR;goto fail;}
            if(!admitted) continue;
            if (tag.length == SIZE_MAX) { status = QJS_INTL_OVERFLOW; goto fail; }
            copy = a->malloc(a->opaque, tag.length + 1);
            if (!copy) { status = QJS_INTL_NO_MEMORY; goto fail; }
            memcpy(copy, tag.data, tag.length); copy[tag.length] = 0;
            out->items[out->count++] = (QJSIntlBytes){copy,tag.length};
        }
        if (!pass && !out->count) { status = QJS_INTL_UNSUPPORTED; goto fail; }
    }
    return QJS_INTL_OK;
 fail:
    qjs_intl_tag_list_clear(p, out);
    return status;
}
