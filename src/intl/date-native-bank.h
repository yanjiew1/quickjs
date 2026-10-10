/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATE_NATIVE_BANK_H
#define QJS_INTL_DATE_NATIVE_BANK_H
#include "date-native.h"

/* These are provider ordinals, not JSClassID or frontend field ordinals.
 * The frontend maps each actual internal-slot brand explicitly. A proxy or
 * object with the right prototype cannot be assigned a Temporal kind.
 */
typedef enum QJSIntlDateValueKind {
    QJS_DATE_VALUE_NUMBER,
    QJS_DATE_VALUE_PLAIN_DATE,
    QJS_DATE_VALUE_PLAIN_YEAR_MONTH,
    QJS_DATE_VALUE_PLAIN_MONTH_DAY,
    QJS_DATE_VALUE_PLAIN_TIME,
    QJS_DATE_VALUE_PLAIN_DATE_TIME,
    QJS_DATE_VALUE_INSTANT,
    QJS_DATE_VALUE_BANK_COUNT,
    QJS_DATE_VALUE_ZONED_DATE_TIME = QJS_DATE_VALUE_BANK_COUNT
} QJSIntlDateValueKind;
typedef enum QJSIntlDateRequired {
    QJS_DATE_REQUIRE_DATE, QJS_DATE_REQUIRE_TIME,
    QJS_DATE_REQUIRE_YEAR_MONTH, QJS_DATE_REQUIRE_MONTH_DAY,
    QJS_DATE_REQUIRE_ANY
} QJSIntlDateRequired;
typedef enum QJSIntlDateDefaults {
    QJS_DATE_DEFAULT_DATE, QJS_DATE_DEFAULT_TIME,
    QJS_DATE_DEFAULT_YEAR_MONTH, QJS_DATE_DEFAULT_MONTH_DAY,
    QJS_DATE_DEFAULT_ALL, QJS_DATE_DEFAULT_ZONED_DATE_TIME
} QJSIntlDateDefaults;
typedef struct QJSIntlDateBankOptions {
    QJSIntlDateOptions requested; /* fields BEFORE any Number/date defaults */
    QJSIntlDateRequired number_required;
    QJSIntlDateDefaults number_defaults;
    /* Only Instant's zoned toLocaleString path uses ZONED_DATE_TIME;
     * ordinary DateTimeFormat construction uses ALL.
     */
    QJSIntlDateDefaults instant_defaults;
} QJSIntlDateBankOptions;
typedef struct QJSIntlDateValue {
    QJSIntlDateValueKind kind;
    QJSIntlBytes calendar;        /* public identifier from internal slot */
    union {
        int64_t number_ms;        /* frontend ToNumber then TimeClip */
        QJSTemporalISODate date;  /* Date/YM/MD: exact stored ISO reference */
        QJSTemporalISOTime time;
        QJSTemporalISODateTime datetime;
        QJSTemporalEpochNs instant;
    } value;
} QJSIntlDateValue;
/* Semantic faults use INVALID_ARGUMENT with this extra typed discriminator.
 * Mismatch maps to RangeError; inapplicable/Zoned/mixed maps to TypeError.
 * UNSUPPORTED remains a backend capability error, never a nullable format.
 */
typedef enum QJSIntlDateValueFault {
    QJS_DATE_FAULT_NONE,
    QJS_DATE_FAULT_CALENDAR_MISMATCH,
    QJS_DATE_FAULT_INAPPLICABLE_TYPE,
    QJS_DATE_FAULT_MIXED_TYPE
} QJSIntlDateValueFault;
typedef struct QJSIntlNativeDateBank QJSIntlNativeDateBank;
typedef struct QJSIntlDateValueFormat {
    QJSIntlNativeDate *format;    /* immutable borrowed handle until bank close */
    QJSTemporalEpochNs epoch;
    int is_plain;
} QJSIntlDateValueFormat;
/* All seven slots are prepared atomically from one typed option snapshot.
 * Number/Instant and sufficient styles can share an immutable selected handle.
 * Four slots can be NULL by specification; allocation/selection errors fail
 * the entire open and close every unique prepared handle. Input strings/data
 * are copied by the handles. Environment callbacks/opaque remain borrowed.
 * No JS values, ICU handles, process defaults, or provider dispatch tables.
 */
QJSIntlStatus qjs_intl_native_date_bank_open(const QJSIntlAllocator *,
    const QJSIntlDateData *, const QJSIntlDateBankOptions *,
    const QJSIntlDateEnvironment *, QJSIntlNativeDateBank **);
void qjs_intl_native_date_bank_close(QJSIntlNativeDateBank *);
/* HandleDateTimeValue preparation only: no environment callbacks/rendering.
 * Calendar mismatch precedes the nullable-format check. A successful record
 * retains exact nanoseconds; no plain/Instant value passes through TimeClip.
 * record/fault are required and reset before work.
 */
QJSIntlStatus qjs_intl_native_date_bank_handle(const QJSIntlNativeDateBank *,
    const QJSIntlDateValue *, QJSIntlDateValueFormat *, QJSIntlDateValueFault *);
QJSIntlStatus qjs_intl_native_date_bank_format(const QJSIntlNativeDateBank *,
    const QJSIntlDateValue *, QJSIntlFormatted *, QJSIntlDateValueFault *);
/* SameTemporalType is checked before handling start, then end. Both Numbers
 * are already clipped by the frontend after both ToNumber conversions. No
 * chronological-order rejection. Result owns text/parts and clears through
 * the creating allocator. A fault result is empty and NONE for backend errors.
 */
QJSIntlStatus qjs_intl_native_date_bank_range(const QJSIntlNativeDateBank *,
    const QJSIntlDateValue *, const QJSIntlDateValue *,
    QJSIntlFormatted *, QJSIntlDateValueFault *);
/* Borrowed slot for constructor/resolvedOptions wiring; NULL can mean one
 * of the four specified inapplicable slots. It does not transfer ownership.
 */
const QJSIntlNativeDate *qjs_intl_native_date_bank_slot(
    const QJSIntlNativeDateBank *, QJSIntlDateValueKind);
#endif
