/* CONTRACT REVISION PROPOSAL ONLY. Internal plain C contract; not embedding ABI.
 * No implementation is supplied or activated by this packet.
 */
#ifndef QJS_INTL_PROVIDER_H
#define QJS_INTL_PROVIDER_H
#include <stddef.h>
#include <stdint.h>

typedef enum QJSIntlStatus {
    QJS_INTL_OK = 0,
    QJS_INTL_NO_MEMORY,
    QJS_INTL_INVALID_ARGUMENT,
    QJS_INTL_UNSUPPORTED,
    QJS_INTL_DATA_ERROR,
    QJS_INTL_OVERFLOW
} QJSIntlStatus;

typedef enum QJSIntlBackend {
    QJS_INTL_BACKEND_ICU = 1,
    QJS_INTL_BACKEND_NATIVE = 2
} QJSIntlBackend;

typedef enum QJSIntlService {
    QJS_INTL_LOCALE = 0,
    QJS_INTL_COLLATOR,
    QJS_INTL_SEGMENTER,
    QJS_INTL_NUMBER_FORMAT,
    QJS_INTL_DATE_TIME_FORMAT,
    QJS_INTL_PLURAL_RULES,
    QJS_INTL_LIST_FORMAT,
    QJS_INTL_RELATIVE_TIME_FORMAT,
    QJS_INTL_DISPLAY_NAMES,
    QJS_INTL_DURATION_FORMAT,
    QJS_INTL_COLLATOR_SEARCH, /* separate availability; explicit wire mapping */
    QJS_INTL_SERVICE_COUNT
} QJSIntlService;

typedef struct QJSIntlAllocator {
    void *opaque;
    void *(*malloc)(void *opaque, size_t size);
    void *(*realloc)(void *opaque, void *ptr, size_t size);
    void (*free)(void *opaque, void *ptr);
} QJSIntlAllocator;
/* Never call realloc with size 0; clear/free functions accept NULL. */

typedef struct QJSIntlBytes {
    const char *data;
    size_t length;
} QJSIntlBytes;
typedef struct QJSIntlUTF16 {
    const uint16_t *data;
    size_t length;
} QJSIntlUTF16;
/* Input slices are borrowed for a call unless an open operation documents a
 * copied snapshot. Embedded NUL and unpaired surrogates are preserved.
 * Every size addition/multiplication is checked before allocation.
 */

typedef enum QJSIntlPartType {
    QJS_INTL_PART_LITERAL = 0,
    QJS_INTL_PART_ELEMENT,
    QJS_INTL_PART_INTEGER,
    QJS_INTL_PART_FRACTION,
    QJS_INTL_PART_GROUP,
    QJS_INTL_PART_DECIMAL,
    QJS_INTL_PART_PLUS_SIGN,
    QJS_INTL_PART_MINUS_SIGN,
    QJS_INTL_PART_PERCENT_SIGN,
    QJS_INTL_PART_CURRENCY,
    QJS_INTL_PART_UNIT,
    QJS_INTL_PART_COMPACT,
    QJS_INTL_PART_EXPONENT_INTEGER,
    QJS_INTL_PART_EXPONENT_SEPARATOR,
    QJS_INTL_PART_EXPONENT_MINUS_SIGN,
    QJS_INTL_PART_NAN,
    QJS_INTL_PART_INFINITY,
    QJS_INTL_PART_APPROXIMATELY_SIGN,
    QJS_INTL_PART_ERA,
    QJS_INTL_PART_YEAR,
    QJS_INTL_PART_RELATED_YEAR,
    QJS_INTL_PART_YEAR_NAME,
    QJS_INTL_PART_MONTH,
    QJS_INTL_PART_DAY,
    QJS_INTL_PART_WEEKDAY,
    QJS_INTL_PART_DAY_PERIOD,
    QJS_INTL_PART_HOUR,
    QJS_INTL_PART_MINUTE,
    QJS_INTL_PART_SECOND,
    QJS_INTL_PART_FRACTIONAL_SECOND,
    QJS_INTL_PART_TIME_ZONE_NAME
} QJSIntlPartType;
typedef enum QJSIntlPartSource {
    QJS_INTL_SOURCE_SINGLE = 0,
    QJS_INTL_SOURCE_START_RANGE,
    QJS_INTL_SOURCE_END_RANGE,
    QJS_INTL_SOURCE_SHARED
} QJSIntlPartSource;
typedef struct QJSIntlPart {
    size_t start;                 /* UTF-16 code-unit offset, inclusive */
    size_t end;                   /* UTF-16 code-unit offset, exclusive */
    QJSIntlPartType type;
    QJSIntlPartSource source;
    QJSIntlBytes unit;            /* optional duration unit identifier */
} QJSIntlPart;
typedef struct QJSIntlFormatted {
    uint16_t *text;               /* owned; explicit length, no NUL required */
    size_t length;
    QJSIntlPart *parts;           /* owned; sorted, non-overlapping spans */
    size_t part_count;
} QJSIntlFormatted;
/* A successful parts result partitions the complete text, including literals.
 * Result fields are zeroed before work; failure leaves a clearable result.
 * Unit labels are borrowed from the provider until result clear.
 */
typedef struct QJSIntlProvider QJSIntlProvider;
void qjs_intl_formatted_clear(QJSIntlProvider *, QJSIntlFormatted *);

typedef enum QJSIntlMathematicalKind {
    QJS_INTL_FINITE = 0,
    QJS_INTL_NAN,
    QJS_INTL_POSITIVE_INFINITY,
    QJS_INTL_NEGATIVE_INFINITY,
    QJS_INTL_NEGATIVE_ZERO
} QJSIntlMathematicalKind;
typedef struct QJSIntlMathematicalValue {
    QJSIntlMathematicalKind kind;
    QJSIntlBytes decimal;         /* exact ASCII decimal incl. sign/exponent */
} QJSIntlMathematicalValue;
/* Number/BigInt/string coercion stays in the engine. Decimal operations must
 * not round through double or expand unbounded exponent zero runs.
 */

typedef struct QJSIntlTagList {
    QJSIntlBytes *items;          /* owned array and tag strings */
    size_t count;
} QJSIntlTagList;
void qjs_intl_tag_list_clear(QJSIntlProvider *, QJSIntlTagList *);
void qjs_intl_owned_tag_clear(QJSIntlProvider *, char *);

typedef enum QJSIntlLocaleInfoField {
    QJS_INTL_LOCALE_CALENDARS = 0,
    QJS_INTL_LOCALE_COLLATIONS,
    QJS_INTL_LOCALE_HOUR_CYCLES,
    QJS_INTL_LOCALE_NUMBERING_SYSTEMS,
    QJS_INTL_LOCALE_TIME_ZONES,
    QJS_INTL_LOCALE_TEXT_INFO,
    QJS_INTL_LOCALE_WEEK_INFO,
    QJS_INTL_LOCALE_FIRST_DAY_OF_WEEK
} QJSIntlLocaleInfoField;
typedef enum QJSIntlLocaleDirection {
    QJS_INTL_LOCALE_DIRECTION_UNDEFINED = 0,
    QJS_INTL_LOCALE_DIRECTION_LTR = 1,
    QJS_INTL_LOCALE_DIRECTION_RTL = 2
} QJSIntlLocaleDirection;

typedef struct QJSIntlLocaleInfoRequest {
    QJSIntlBytes locale;
    /* Actual constructor internal slots, not ResolveLocale values. NULL/0
     * means undefined; non-NULL/0 means the present empty string. The frontend
     * copies its already canonical ca/co/hc/nu/fw slots here. They are not
     * inferred from the tag, because supported locale extension keys and
     * constructor options belong to the frontend. Slots are syntax checked;
     * unknown well-formed inherited values survive. */
    QJSIntlBytes calendar, collation, hour_cycle, numbering_system, first_day;
} QJSIntlLocaleInfoRequest;

typedef struct QJSIntlLocaleInfoResult {
    QJSIntlTagList list;         /* owned array and separately owned strings */
    char *value;                /* owned firstDayOfWeek slot, possibly empty */
    unsigned int defined;       /* distinguishes undefined from an empty list;
                                * TextInfo: direction value definedness,
                                * frontend always creates its direction property */
    QJSIntlLocaleDirection direction;
    uint8_t first_day;          /* ISO Monday=1 ... Sunday=7 */
    uint8_t weekend_mask;       /* frontend expands bit0..6 in ascending order */
    /* Latest WeekInfo has no minimalDays field. */
} QJSIntlLocaleInfoResult;

void qjs_intl_locale_info_result_clear(QJSIntlProvider *, QJSIntlLocaleInfoResult *);

typedef struct QJSIntlLocaleOps {
    QJSIntlStatus (*canonicalize)(QJSIntlProvider *, QJSIntlBytes, char **);
    QJSIntlStatus (*canonicalize_uvalue)(QJSIntlProvider *, QJSIntlBytes key,
                                         QJSIntlBytes value, char **);
    QJSIntlStatus (*maximize)(QJSIntlProvider *, QJSIntlBytes, char **);
    QJSIntlStatus (*minimize)(QJSIntlProvider *, QJSIntlBytes, char **);
    QJSIntlStatus (*info_get)(QJSIntlProvider *, const QJSIntlLocaleInfoRequest *,
                              QJSIntlLocaleInfoField, QJSIntlLocaleInfoResult *);
    QJSIntlStatus (*available)(QJSIntlProvider *, QJSIntlService,
                              QJSIntlTagList *);
    QJSIntlStatus (*key_values)(QJSIntlProvider *, QJSIntlService,
                               QJSIntlBytes locale, QJSIntlBytes key,
                               QJSIntlTagList *);
    QJSIntlStatus (*time_zones)(QJSIntlProvider *, QJSIntlBytes explicit_region,
                               QJSIntlTagList *);
} QJSIntlLocaleOps;
/* Locale operations accept validated canonical IDs except canonicalize.
 * info_get evaluates only the requested field and honors rg/sd/fw metadata
 * where required. Request slots preserve undefined versus present empty. These operations
 * contain no JS getters and return neutral errors only.
 */

/* Service options and operations are typed in their own plain C headers.
 * Complete the named structs per service, preserving the already validated
 * frontend option record. A generic void *options API is deliberately absent.
 */
typedef struct QJSIntlCollatorOps QJSIntlCollatorOps;
typedef struct QJSIntlSegmenterOps QJSIntlSegmenterOps;
typedef struct QJSIntlNumberOps QJSIntlNumberOps;
typedef struct QJSIntlDateOps QJSIntlDateOps;
typedef struct QJSIntlPluralOps QJSIntlPluralOps;
typedef struct QJSIntlListOps QJSIntlListOps;
typedef struct QJSIntlRelativeTimeOps QJSIntlRelativeTimeOps;
typedef struct QJSIntlDisplayNamesOps QJSIntlDisplayNamesOps;
typedef struct QJSIntlDurationOps QJSIntlDurationOps;
typedef struct QJSIntlCalendarOps QJSIntlCalendarOps;
typedef struct QJSIntlZoneOps QJSIntlZoneOps;
typedef struct QJSIntlCaseOps QJSIntlCaseOps;

/* Optional dual-provider runtime selection proposal. Initial compile-selected
 * builds call direct provider entry points below; no operation table lookup is
 * required on the normal path. Enable this only after a separate complexity/
 * freeze/lifecycle review by the integrating owner.
 */
#ifdef QJS_INTL_RUNTIME_SELECT
typedef struct QJSIntlOperations {
    const QJSIntlLocaleOps *locale;
    const QJSIntlCollatorOps *collator;
    const QJSIntlSegmenterOps *segmenter;
    const QJSIntlNumberOps *number;
    const QJSIntlDateOps *date;
    const QJSIntlPluralOps *plural;
    const QJSIntlListOps *list;
    const QJSIntlRelativeTimeOps *relative_time;
    const QJSIntlDisplayNamesOps *display_names;
    const QJSIntlDurationOps *duration;
    const QJSIntlCalendarOps *calendar;
    const QJSIntlZoneOps *zone;
    const QJSIntlCaseOps *case_mapping;
} QJSIntlOperations;
#endif
/* Each service open owns an opaque handle; close is provider-specific.
 * A handle borrows its creating provider. No handle crosses providers.
 * Release native Intl requires every required operations table complete.
 */

typedef struct QJSIntlDataVersions {
    QJSIntlBytes provider;
    QJSIntlBytes unicode;
    QJSIntlBytes cldr;
    QJSIntlBytes collation;
    QJSIntlBytes tzdata;
    QJSIntlBytes input_manifest_sha256;
    uint32_t schema_version;
} QJSIntlDataVersions;

struct QJSTzProvider;
typedef struct QJSIntlProviderConfig {
    QJSIntlBackend backend;
    QJSIntlAllocator allocator;
    QJSIntlBytes default_locale;
    QJSIntlBytes default_time_zone;
    struct QJSTzProvider *native_time_zones; /* borrowed runtime snapshot */
} QJSIntlProviderConfig;
/* Runtime copies/canonicalizes defaults on construction and owns the shared
 * timezone snapshot even with CONFIG_INTL_NATIVE and CONFIG_TEMPORAL=n.
 * ICU uses its provider-local zone adapter. Runtime frees all service handles,
 * then Intl provider, then shared native timezone provider.
 */
QJSIntlStatus qjs_intl_provider_new(const QJSIntlProviderConfig *,
                                    QJSIntlProvider **out);
void qjs_intl_provider_free(QJSIntlProvider *);
#ifdef QJS_INTL_RUNTIME_SELECT
const QJSIntlOperations *qjs_intl_provider_operations(const QJSIntlProvider *);
#endif
/* Direct symbols have exactly one compile-selected ICU/native definition.
 * Backend-specific implementation names stay private to their owner modules.
 */
QJSIntlStatus qjs_intl_locale_canonicalize(QJSIntlProvider *, QJSIntlBytes,
                                           char **);
QJSIntlStatus qjs_intl_locale_canonicalize_uvalue(QJSIntlProvider *,
                          QJSIntlBytes key, QJSIntlBytes value, char **);
QJSIntlStatus qjs_intl_locale_maximize(QJSIntlProvider *, QJSIntlBytes, char **);
QJSIntlStatus qjs_intl_locale_minimize(QJSIntlProvider *, QJSIntlBytes, char **);
QJSIntlStatus qjs_intl_locale_info_get(QJSIntlProvider *,
                                  const QJSIntlLocaleInfoRequest *,
                                  QJSIntlLocaleInfoField, QJSIntlLocaleInfoResult *);
QJSIntlStatus qjs_intl_locale_available(QJSIntlProvider *, QJSIntlService,
                                       QJSIntlTagList *);
QJSIntlStatus qjs_intl_locale_key_values(QJSIntlProvider *, QJSIntlService,
                          QJSIntlBytes locale, QJSIntlBytes key, QJSIntlTagList *);
QJSIntlStatus qjs_intl_locale_time_zones(QJSIntlProvider *,
                          QJSIntlBytes explicit_region, QJSIntlTagList *);
/* The engine returns undefined for Locale.getTimeZones when the tag has no
 * explicit region. It does not use likely-subtag inference for that method.
 */
const QJSIntlDataVersions *qjs_intl_provider_versions(const QJSIntlProvider *);
QJSIntlBytes qjs_intl_provider_default_locale(const QJSIntlProvider *);
QJSIntlBytes qjs_intl_provider_default_time_zone(const QJSIntlProvider *);

#endif /* QJS_INTL_PROVIDER_H */
