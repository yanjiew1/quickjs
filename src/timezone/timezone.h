/* Native IANA time zones. Copyright (c) 2026 Yan-Jie Wang. MIT. */
#ifndef QUICKJS_TIMEZONE_H
#define QUICKJS_TIMEZONE_H
#include <stddef.h>
#include <stdint.h>

typedef struct QJSTimeZone QJSTimeZone;
typedef struct QJSTzProvider QJSTzProvider;
enum {
    QJS_TZ_OK, QJS_TZ_ABSENT, QJS_TZ_INVALID, QJS_TZ_MEMORY, QJS_TZ_RANGE
};
/* Allocation results must be aligned for int64_t and object/function
   pointers. The exact returned pointer is passed to deallocate. Allocator
   callbacks are copied; opaque must remain valid until the owner closes. */
typedef struct QJSTzAllocator {
    void *opaque;
    void *(*allocate)(void *, size_t);
    void (*deallocate)(void *, void *);
} QJSTzAllocator;
/* read returns borrowed immutable bytes until release. A successful read
   always gets exactly one release, including parser failure. Memory errors
   propagate; other source errors permit the compiled fallback. */
typedef struct QJSTzSource {
    void *opaque;
    int (*read)(void *, const char *, const unsigned char **, size_t *);
    void (*release)(void *, const unsigned char *, size_t);
} QJSTzSource;
enum { QJS_TZ_SYSTEM, QJS_TZ_EMBEDDED };
int qjs_tz_resolve(const char *, size_t, const char **identifier,
                  const char **primary);
size_t qjs_tz_identifier_count(void);
const char *qjs_tz_identifier_at(size_t);
/* NULL source uses system zoneinfo; NULL allocator uses malloc/free.
   An explicit source can implement a sandbox or embedding policy. */
int qjs_tz_open(QJSTimeZone **, const char *, size_t,
                const QJSTzSource *, const QJSTzAllocator *);
int qjs_tz_open_embedded(QJSTimeZone **, const char *, size_t,
                         const QJSTzAllocator *);
void qjs_tz_close(QJSTimeZone *);
/* One provider belongs to one embedding agent/runtime. It lazily retains
   one immutable data selection per primary until freed. Open returns a
   borrowed snapshot: never close it. The owner serializes provider access. */
int qjs_tz_provider_create(QJSTzProvider **, const QJSTzSource *, const QJSTzAllocator *);
void qjs_tz_provider_free(QJSTzProvider *);
int qjs_tz_provider_open(QJSTzProvider *, const char *, size_t, const QJSTimeZone **);
int qjs_tz_origin(const QJSTimeZone *);
int qjs_tz_offset(const QJSTimeZone *, int64_t epoch_seconds, int32_t *);
/* TZif daylight classification is independent of the signed daylight
   adjustment. Historical TZif types omit STDOFF/SAVE: their adjustment
   is unknown for either flag value. POSIX tails prove both. */
typedef struct QJSTzInfo {
    int32_t offset_seconds, daylight_offset_seconds;
    int daylight, daylight_offset_known;
} QJSTzInfo;
int qjs_tz_info(const QJSTimeZone *, int64_t epoch_seconds, QJSTzInfo *);
/* Prove constant total offset, classification and known signed daylight
   adjustment on the closed interval. False includes an unknown adjustment.
   Raw DST/base-policy transitions are inspected independently of Temporal. */
int qjs_tz_name_stable(const QJSTimeZone *, int64_t from_seconds,
                       int64_t through_seconds, int *proven);
/* Return the offsets bounding a wall time. Equal offsets mean an ordinary
   time. In a gap or overlap, offsets[0] is before and offsets[1] is after. */
int qjs_tz_local_offsets(const QJSTimeZone *, int64_t wall_seconds,
                         int32_t offsets[2]);
int qjs_tz_transition(const QJSTimeZone *, int64_t epoch_seconds, int next,
                      int inclusive, int64_t *, int *found);
/* Copies a primary host identifier; this never sets TZ or calls tzset.
   Unknown host configurations fall back to UTC. */
int qjs_tz_system_identifier(char *, size_t);
#ifdef _WIN32
/* Embedding Windows host discovery, backed by compiled CLDR mapping. */
int qjs_tz_windows_system_identifier(char *, size_t);
#endif
#endif
