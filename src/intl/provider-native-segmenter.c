/* Native Segmenter policy: installed locales use Unicode18 default UAX29 rules.
 * No dictionary or language tailoring is claimed. No additional EP table or
 * generated metadata definition is compiled here. */
#include "provider-native-segmenter.h"
#include <string.h>
struct QJSIntlProviderSegmenter {
    QJSIntlAllocator allocator;
    QJSIntlSegmenterUnicode *unicode;
    QJSIntlNativeSegmenterData *data;
    QJSIntlNativeSegmenter *handle;
};
void qjs_intl_native_provider_segmenter_close(QJSIntlProviderSegmenter *s)
{
    QJSIntlAllocator a;
    if (!s) return;
    a = s->allocator;
    qjs_intl_native_segmenter_close(s->handle);
    qjs_intl_native_segmenter_data_close(s->data);
    qjs_intl_native_segmenter_unicode_close(s->unicode);
    a.free(a.opaque, s);
}
QJSIntlStatus qjs_intl_native_provider_segmenter_open(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlSegmentGranularity granularity,
    QJSIntlProviderSegmenter **out)
{
    const QJSIntlAllocator *a = qjs_intl_native_provider_allocator(p);
    const QJSIntlDataView *view = qjs_intl_native_provider_view(p);
    QJSIntlProviderSegmenter *s;
    QJSIntlExtendedPictographicSource ep;
    QJSIntlSegmentPropertySource properties;
    QJSIntlStatus status;
    uint32_t row;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !view) return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_native_provider_locale_row(p, locale, &row);
    if (status != QJS_INTL_OK) return status;
    s = a->malloc(a->opaque, sizeof(*s));
    if (!s) return QJS_INTL_NO_MEMORY;
    memset(s, 0, sizeof(*s));
    s->allocator = *a;
    status = qjs_intl_native_segmenter_unicode_open(a, &s->unicode);
    if (status != QJS_INTL_OK) goto fail;
    ep = qjs_intl_native_segmenter_unicode_ep(s->unicode);
    status = qjs_intl_native_segmenter_data_open(a, view, &ep, &s->data);
    if (status != QJS_INTL_OK) goto fail;
    properties = qjs_intl_native_segmenter_data_properties(s->data);
    status = qjs_intl_native_segmenter_open_default(a, &properties, granularity,
                                                  &s->handle);
    if (status != QJS_INTL_OK) goto fail;
    *out = s;
    return QJS_INTL_OK;
fail:
    qjs_intl_native_provider_segmenter_close(s);
    return status;
}
QJSIntlStatus qjs_intl_native_provider_segments_new(QJSIntlProviderSegmenter *s,
    QJSIntlUTF16 input, QJSIntlNativeSegments **out)
{
    if (!s) { if (out) *out = NULL; return QJS_INTL_INVALID_ARGUMENT; }
    return qjs_intl_native_segments_new(s->handle, input, out);
}
QJSIntlStatus qjs_intl_native_provider_segmenter_locale_available(QJSIntlProvider *p,
    QJSIntlBytes locale, uint32_t row)
{
    QJSIntlProviderSegmenter *s = NULL;
    QJSIntlStatus status;
    unsigned i;
    (void)row; /* segmenter_open resolves this same authenticated tag. */
    for (i = 0; i < 3; i++) {
        status = qjs_intl_native_provider_segmenter_open(p,
            locale, (QJSIntlSegmentGranularity)i, &s);
        if (status != QJS_INTL_OK) return status;
        qjs_intl_native_provider_segmenter_close(s);
        s = NULL;
    }
    return QJS_INTL_OK;
}
