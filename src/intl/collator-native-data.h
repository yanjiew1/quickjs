/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATOR_NATIVE_DATA_H
#define QJS_INTL_COLLATOR_NATIVE_DATA_H
#include "provider.h"
#include "data/native-data-reader.h"

/* Canonical IDs/layout live only in metadata-data.h, extended by the paired
 * metadata-data-collation.patch. No runtime XML/JSON, native-struct casts,
 * duplicated Unicode tables, or per-TU data/helper definitions.
 */
typedef struct QJSIntlCollationData {
    QJSIntlDataSection nodes, ces, implicit, digits, capabilities, config;
    uint32_t numeric_primary, max_depth;
} QJSIntlCollationData;

/* A successfully opened generic view is required. This validator additionally
 * verifies every collation span, enum, tree edge and locale reference.
 * Missing atomic extension: UNSUPPORTED; partial/malformed: DATA_ERROR.
 * out is zeroed before work. Returned sections borrow the immutable blob;
 * the caller keeps it alive and unchanged through all handle lifetimes.
 */
QJSIntlStatus qjs_intl_collation_data_init(const QJSIntlDataView *,
                                         QJSIntlCollationData *out);
/* Hook for the generic reader, after its layout and base validation. No
 * sections90..95 is valid; a partially present extension is invalid.
 */
QJSIntlDataStatus qjs_intl_data_validate_collation_extension(
                                                    const QJSIntlDataView *);
#endif
