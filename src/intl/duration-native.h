/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DURATION_NATIVE_H
#define QJS_INTL_DURATION_NATIVE_H
#include "number-native.h"
#include "list-native.h"

enum { QJS_INTL_DURATION_UNIT_COUNT = 10 };
typedef enum QJSIntlDurationStyle {
    QJS_INTL_DURATION_LONG = 0, QJS_INTL_DURATION_SHORT,
    QJS_INTL_DURATION_NARROW, QJS_INTL_DURATION_DIGITAL
} QJSIntlDurationStyle;
typedef enum QJSIntlDurationUnitStyle {
    QJS_INTL_DURATION_UNIT_LONG = 0, QJS_INTL_DURATION_UNIT_SHORT,
    QJS_INTL_DURATION_UNIT_NARROW, QJS_INTL_DURATION_NUMERIC,
    QJS_INTL_DURATION_TWO_DIGIT, QJS_INTL_DURATION_FRACTIONAL
} QJSIntlDurationUnitStyle;
typedef struct QJSIntlDurationUnitOptions {
    QJSIntlDurationUnitStyle style;
    uint8_t always;
} QJSIntlDurationUnitOptions;
typedef struct QJSIntlDurationOptions {
    QJSIntlDurationStyle style;
    QJSIntlDurationUnitOptions units[QJS_INTL_DURATION_UNIT_COUNT];
    int8_t fractional_digits; /* -1 undefined; otherwise 0..9 */
    size_t maximum_output_length;
} QJSIntlDurationOptions;
/* Frontend has already performed ToDurationRecord/IsValidDuration, including
 * exact normalized-seconds bounds. Magnitudes are normalized unsigned ASCII
 * integers (zero is "0"), in year/month/week/day/hour/minute/second/
 * millisecond/microsecond/nanosecond order. sign is -1,0,+1; sign0 iff every
 * magnitude is zero. No JS getters/coercions or finite Number conversion here.
 */
typedef struct QJSIntlDurationRecord {
    QJSIntlBytes magnitudes[QJS_INTL_DURATION_UNIT_COUNT];
    int8_t sign;
} QJSIntlDurationRecord;
/* Shared NumberFormat owner factory. open must return a fully owned standard
 * NumberFormat handle matching exactly these options, retaining any cardinal
 * handle until close. On error it sets *out=NULL and releases partial state.
 * close releases NumberFormat before its cardinal/data dependencies.
 * open_opaque is borrowed only during open; close_opaque lives until close.
 * No factory call during formatting.
 * Locale and numbering-system selection belong to this factory's snapshot.
 * DurationFormat only calls shared number_format/number_result_clear APIs.
 */
typedef struct QJSIntlDurationNumberFactory {
    void *open_opaque, *close_opaque;
    QJSIntlStatus (*open)(void *, const QJSIntlAllocator *,
                         const QJSIntlNumberOptions *, QJSIntlNativeNumber **);
    void (*close)(void *, QJSIntlNativeNumber *);
} QJSIntlDurationNumberFactory;
typedef struct QJSIntlDurationData {
    QJSIntlDurationNumberFactory number;
    QJSIntlListTemplates list; /* used unless open_list is supplied */
    void *list_open_opaque; /* borrowed only during duration open */
    QJSIntlStatus (*open_list)(void *, const QJSIntlAllocator *,
                              QJSIntlListStyle, QJSIntlNativeList **);
    /* open_list supplies an owned unit ListFormat using shared ListFormat.
     * On failure releases partial state and sets *out=NULL. */
    QJSIntlUTF16 hour_minute_separator, minute_second_separator;
} QJSIntlDurationData;
typedef struct QJSIntlNativeDuration QJSIntlNativeDuration;
/* Options are resolved internal slots: frontend applies TwoDigitHours and
 * prevStyle propagation before open. Open snapshots options/list/separators,
 * owns all number and list handles. Missing required dependency -> caller's
 * explicit UNSUPPORTED/DATA_ERROR, never fabricated English defaults.
 */
QJSIntlStatus qjs_intl_native_duration_open(const QJSIntlAllocator *,
    const QJSIntlDurationOptions *, const QJSIntlDurationData *, QJSIntlNativeDuration **);
void qjs_intl_native_duration_close(QJSIntlNativeDuration *);
QJSIntlStatus qjs_intl_native_duration_resolved_options(const QJSIntlNativeDuration *,
                                                      QJSIntlDurationOptions *);
QJSIntlStatus qjs_intl_native_duration_format(const QJSIntlNativeDuration *,
    const QJSIntlDurationRecord *, QJSIntlFormatted *);
/* Result text/parts own allocator storage and survive handle close. Unit
 * labels are immutable module strings. clear with original allocator/opaque.
 * Every failed format leaves all-zero output. Fresh disjoint output required.
 */
void qjs_intl_native_duration_result_clear(const QJSIntlAllocator *, QJSIntlFormatted *);
#endif
