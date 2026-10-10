/* Private plain C reader for the versioned native Intl wire schema.
 * This is not an embedding API. No allocation, engine, or ICU dependency.
 */
#ifndef QJS_INTL_NATIVE_DATA_READER_H
#define QJS_INTL_NATIVE_DATA_READER_H
#include <stddef.h>
#include <stdint.h>
#include "intl/metadata-data.h"

typedef enum QJSIntlDataStatus {
    QJS_INTL_DATA_OK = 0,
    QJS_INTL_DATA_INVALID_ARGUMENT,
    QJS_INTL_DATA_INVALID,
    QJS_INTL_DATA_UNSUPPORTED_VERSION,
    QJS_INTL_DATA_NOT_FOUND
} QJSIntlDataStatus;

typedef struct QJSIntlDataSlice {
    const unsigned char *data;
    size_t length;
} QJSIntlDataSlice;

typedef struct QJSIntlDataView {
    const unsigned char *data;
    size_t length;
} QJSIntlDataView;

typedef struct QJSIntlDataSection {
    QJSIntlDataSlice bytes;
    uint32_t id;
    uint32_t record_width;
    uint32_t record_count;
} QJSIntlDataSection;

/* On failure, every non-NULL output is zeroed before returning. Every output
 * pointer is required. NULL data/view/section pointers are invalid even with
 * zero length. Empty successful slices point inside the borrowed blob, which
 * needs no NUL terminator. The blob must remain alive and unchanged for the
 * view and all returned sections/slices. Caller-owned view/section structures
 * must also remain unchanged between successful creation and access. Output
 * objects must not overlap input objects or the immutable blob. Accessors
 * require a view/section returned by successful open/section operations.
 * record_string requires a section borrowed from that same view.
 */
/* Schemas1.0,1.1,1.2 and1.3 and basic data-version majors are checked.
 * The generic reader does not authenticate the input digest or assert service
 * completeness. Provider activation separately verifies table compatibility.
 * Optional Plural30..32 is atomic, with exact grammar/span validation.
 * Reader links plural-data-validation.c and the adopted pure plural.c.
 * Optional DisplayNames rows require valid section16 locale references.
 * Any Segmenter60..65 requires nonempty60..63 and exactly3 rule65 rows;
 * EP mode1 requires64, mode2 forbids64. Presence never sets service coverage.
 */
QJSIntlDataStatus qjs_intl_data_open(const void *data, size_t length,
                                    QJSIntlDataView *out);
QJSIntlDataStatus qjs_intl_data_section(const QJSIntlDataView *view,
                                       uint32_t id, QJSIntlDataSection *out);
QJSIntlDataStatus qjs_intl_data_record(const QJSIntlDataSection *section,
                                      uint32_t index, QJSIntlDataSlice *out);
QJSIntlDataStatus qjs_intl_data_record_u32(const QJSIntlDataSection *section,
                                          uint32_t index, uint32_t field_offset,
                                          uint32_t *out);
/* String offsets are relative to the pool. Empty is exactly offset0/length0.
 * Nonempty refs begin after a pool NUL and end immediately before a pool NUL;
 * embedded NUL is forbidden. Identical refs may be shared by many records.
 * This wire termination does not require callers to use C string functions.
 */
QJSIntlDataStatus qjs_intl_data_string(const QJSIntlDataView *view,
                                      uint32_t offset, uint32_t length,
                                      QJSIntlDataSlice *out);
QJSIntlDataStatus qjs_intl_data_record_string(const QJSIntlDataView *view,
                                             const QJSIntlDataSection *section,
                                             uint32_t index,
                                             uint32_t field_offset,
                                             QJSIntlDataSlice *out);
#endif /* QJS_INTL_NATIVE_DATA_READER_H */
