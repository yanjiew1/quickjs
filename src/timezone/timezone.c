/* System zoneinfo first, one compiled QJTZ fallback. No global TZ state.
   Copyright (c) 2026 Yan-Jie Wang. MIT license. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "private.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifndef QJS_TZ_SYSTEM_DIR
#define QJS_TZ_SYSTEM_DIR "/usr/share/zoneinfo"
#endif
struct QJSTimeZone {
    QJSTzAllocator allocator;
    const QJSTzRecord *record;
    int origin;
    QJSTzifState state;
    unsigned char *owned_bytes;
};
struct QJSTzProvider {
    QJSTzAllocator allocator;
    QJSTzSource source;
    int has_source;
    QJSTimeZone *zones[];
};
static unsigned char lower(unsigned char c)
{ return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
static const QJSTzRecord *find_record(const char *text, size_t length)
{
    size_t i, j;
    if (!text || !length || length >= 256 || memchr(text, 0, length)) return NULL;
    for (i = 0; i < qjs_tz_record_count; i++) {
        const char *name = qjs_tz_records[i].identifier;
        if (strlen(name) != length) continue;
        for (j = 0; j < length && lower(text[j]) == lower(name[j]); j++) {}
        if (j == length) return &qjs_tz_records[i];
    }
    return NULL;
}
int qjs_tz_resolve(const char *text, size_t length, const char **id, const char **primary)
{
    const QJSTzRecord *record = find_record(text, length);
    if (!record) return QJS_TZ_INVALID;
    if (id) *id = record->identifier;
    if (primary) *primary = record->primary;
    return QJS_TZ_OK;
}
size_t qjs_tz_identifier_count(void) { return qjs_tz_record_count; }
const char *qjs_tz_identifier_at(size_t i)
{ return i < qjs_tz_record_count ? qjs_tz_records[i].identifier : NULL; }
static void *default_allocate(void *opaque, size_t size)
{ (void)opaque; return malloc(size); }
static void default_deallocate(void *opaque, void *p)
{ (void)opaque; free(p); }
static int system_read(void *opaque, const char *id, const unsigned char **bytes, size_t *size)
{
    char path[sizeof(QJS_TZ_SYSTEM_DIR) + 256];
    FILE *file;
    unsigned char *data;
    size_t length;
    int extra, count;
    (void)opaque;
    /* ID comes only from generated Zone/Link metadata, never an arbitrary
       user path or POSIX TZ expression. Bound memory regardless of input. */
    count = snprintf(path, sizeof path, "%s/%s", QJS_TZ_SYSTEM_DIR, id);
    if (count < 0 || count >= (int)sizeof path)
        return QJS_TZ_INVALID;
    file = fopen(path, "rb");
    if (!file) return errno == ENOMEM ? QJS_TZ_MEMORY : QJS_TZ_ABSENT;
    data = malloc(QJS_TZ_MAX_FILE_SIZE);
    if (!data) { fclose(file); return QJS_TZ_MEMORY; }
    errno = 0;
    length = fread(data, 1, QJS_TZ_MAX_FILE_SIZE, file);
    if (ferror(file)) {
        int error = errno == ENOMEM ? QJS_TZ_MEMORY : QJS_TZ_INVALID;
        fclose(file); free(data); return error;
    }
    errno = 0;
    extra = fgetc(file);
    if (ferror(file) || extra != EOF) {
        int error = ferror(file) && errno == ENOMEM ? QJS_TZ_MEMORY : QJS_TZ_INVALID;
        fclose(file); free(data); return error;
    }
    if (fclose(file)) {
        int error = errno == ENOMEM ? QJS_TZ_MEMORY : QJS_TZ_INVALID;
        free(data); return error;
    }
    *bytes = data; *size = length;
    return QJS_TZ_OK;
}
static void system_release(void *opaque, const unsigned char *bytes, size_t size)
{ (void)opaque; (void)size; free((void *)bytes); }
int qjs_tz_open(QJSTimeZone **output, const char *text, size_t length,
                const QJSTzSource *source, const QJSTzAllocator *allocator)
{
    static const QJSTzAllocator defaults = {NULL, default_allocate, default_deallocate};
    static const QJSTzSource system = {NULL, system_read, system_release};
    const QJSTzRecord *record = find_record(text, length);
    QJSTimeZone *zone;
    const unsigned char *bytes = NULL;
    size_t size = 0;
    int error;
    if (!output || !record) return QJS_TZ_INVALID;
    if (!allocator) allocator = &defaults;
    if (!source) source = &system;
    if (!allocator->allocate || !allocator->deallocate || !source->read ||
        !source->release || record->offset > qjs_tz_embedded_size ||
        record->length > qjs_tz_embedded_size - record->offset) return QJS_TZ_INVALID;
    zone = allocator->allocate(allocator->opaque, sizeof *zone);
    if (!zone) return QJS_TZ_MEMORY;
    zone->allocator = *allocator;
    zone->record = record;
    zone->origin = QJS_TZ_EMBEDDED;
    zone->owned_bytes = NULL;
    /* The embedded reference is an immutable view into the one container.
       A system snapshot owns only its accepted file bytes, using this same
       allocator. It never retains a source's borrowed read buffer. */
    error = qjs_tzif_parse(&zone->state,
        qjs_tz_embedded_data + record->offset, record->length);
    if (error) {
        allocator->deallocate(allocator->opaque, zone);
        return error;
    }
    error = source->read(source->opaque, record->primary, &bytes, &size);
    if (!error) {
        QJSTzifState candidate;
        error = qjs_tzif_parse(&candidate, bytes, size);
        if (!error && !qjs_tzif_system_complete(&candidate, &zone->state)) error = QJS_TZ_INVALID;
        if (!error && !strcmp(record->primary, "UTC") &&
            !qjs_tzif_is_utc(&candidate)) error = QJS_TZ_INVALID;
        if (!error) {
            zone->owned_bytes = allocator->allocate(allocator->opaque, size);
            if (!zone->owned_bytes) error = QJS_TZ_MEMORY;
            else {
                memcpy(zone->owned_bytes, bytes, size);
                error = qjs_tzif_parse(&candidate, zone->owned_bytes, size);
                if (!error) {
                    zone->state = candidate;
                    zone->origin = QJS_TZ_SYSTEM;
                }
            }
        }
        source->release(source->opaque, bytes, size);
    }
    /* Every alias uses its primary's system file and compiled fallback.
       Missing alias links in minimized packages cannot split equal zones. */
    if (error && error != QJS_TZ_MEMORY) {
        if (zone->owned_bytes) {
            allocator->deallocate(allocator->opaque, zone->owned_bytes);
            zone->owned_bytes = NULL;
        }
        error = QJS_TZ_OK;
    }
    if (error) {
        allocator->deallocate(allocator->opaque, zone);
        return error;
    }
    *output = zone;
    return QJS_TZ_OK;
}
void qjs_tz_close(QJSTimeZone *zone)
{
    if (zone) {
        if (zone->owned_bytes)
            zone->allocator.deallocate(zone->allocator.opaque, zone->owned_bytes);
        zone->allocator.deallocate(zone->allocator.opaque, zone);
    }
}
static int absent_read(void *opaque, const char *id, const unsigned char **bytes, size_t *size)
{ (void)opaque; (void)id; (void)bytes; (void)size; return QJS_TZ_ABSENT; }
static void absent_release(void *opaque, const unsigned char *bytes, size_t size)
{ (void)opaque; (void)bytes; (void)size; }
int qjs_tz_open_embedded(QJSTimeZone **output, const char *id, size_t length,
                         const QJSTzAllocator *allocator)
{
    static const QJSTzSource absent = {NULL, absent_read, absent_release};
    return qjs_tz_open(output, id, length, &absent, allocator);
}
int qjs_tz_provider_create(QJSTzProvider **output, const QJSTzSource *source,
                            const QJSTzAllocator *allocator)
{
    static const QJSTzAllocator defaults = {NULL, default_allocate, default_deallocate};
    QJSTzProvider *provider;
    size_t size;
    if (!output) return QJS_TZ_INVALID;
    if (!allocator) allocator = &defaults;
    if (!allocator->allocate || !allocator->deallocate ||
        (source && (!source->read || !source->release)) ||
        qjs_tz_record_count > (SIZE_MAX - sizeof *provider) / sizeof *provider->zones)
        return QJS_TZ_INVALID;
    size = sizeof *provider + qjs_tz_record_count * sizeof *provider->zones;
    provider = allocator->allocate(allocator->opaque, size);
    if (!provider) return QJS_TZ_MEMORY;
    memset(provider, 0, size);
    provider->allocator = *allocator;
    provider->has_source = source != NULL;
    if (source) provider->source = *source;
    *output = provider;
    return QJS_TZ_OK;
}
void qjs_tz_provider_free(QJSTzProvider *provider)
{
    size_t i;
    if (!provider) return;
    for (i = 0; i < qjs_tz_record_count; i++) qjs_tz_close(provider->zones[i]);
    provider->allocator.deallocate(provider->allocator.opaque, provider);
}
int qjs_tz_provider_open(QJSTzProvider *provider, const char *text, size_t length,
                          const QJSTimeZone **output)
{
    const QJSTzRecord *record = find_record(text, length), *primary;
    size_t index;
    int error;
    if (!provider || !output || !record) return QJS_TZ_INVALID;
    primary = find_record(record->primary, strlen(record->primary));
    if (!primary) return QJS_TZ_INVALID;
    index = (size_t)(primary - qjs_tz_records);
    if (!provider->zones[index]) {
        QJSTimeZone *zone;
        error = qjs_tz_open(&zone, primary->identifier, strlen(primary->identifier),
            provider->has_source ? &provider->source : NULL, &provider->allocator);
        if (error) return error; /* failures never poison later allocation retries */
        provider->zones[index] = zone;
    }
    *output = provider->zones[index];
    return QJS_TZ_OK;
}
int qjs_tz_origin(const QJSTimeZone *zone) { return zone->origin; }
int qjs_tz_offset(const QJSTimeZone *zone, int64_t seconds, int32_t *output)
{ return zone ? qjs_tzif_offset(&zone->state, seconds, output) : QJS_TZ_INVALID; }
int qjs_tz_local_offsets(const QJSTimeZone *zone, int64_t wall, int32_t offsets[2])
{ return zone ? qjs_tzif_local_offsets(&zone->state, wall, offsets) : QJS_TZ_INVALID; }
int qjs_tz_transition(const QJSTimeZone *zone, int64_t seconds, int next,
                      int inclusive, int64_t *output, int *found)
{ return zone ? qjs_tzif_transition(&zone->state, seconds, next, inclusive, output, found) : QJS_TZ_INVALID; }
static const char *system_name(const char *text)
{
    const char *id, *primary;
    if (*text == ':') text++;
    if (!qjs_tz_resolve(text, strlen(text), &id, &primary)) return primary;
    /* Resolve a known zoneinfo suffix; arbitrary files never become names. */
    text = strstr(text, "/zoneinfo/");
    if (text && !qjs_tz_resolve(text + 10, strlen(text + 10), &id, &primary)) return primary;
    return NULL;
}
int qjs_tz_system_identifier(char *output, size_t capacity)
{
    const char *name = NULL, *environment = getenv("TZ");
    char buffer[1024];
    if (environment) name = system_name(environment);
#ifndef _WIN32
    if (!environment) {
        ssize_t length = readlink("/etc/localtime", buffer, sizeof buffer - 1);
        if (length > 0 && (size_t)length < sizeof buffer - 1) {
            buffer[length] = 0; name = system_name(buffer);
        }
        if (!name) {
            FILE *file = fopen("/etc/timezone", "r");
            if (file) {
                if (fgets(buffer, sizeof buffer, file)) {
                    size_t length = strcspn(buffer, "\r\n");
                    buffer[length] = 0; name = system_name(buffer);
                }
                fclose(file);
            }
        }
    }
#else
    if (!environment && !qjs_tz_windows_system_identifier(buffer, sizeof buffer))
        name = system_name(buffer);
#endif
    if (!name) name = "UTC";
    if (strlen(name) >= capacity) return QJS_TZ_RANGE;
    memcpy(output, name, strlen(name) + 1);
    return QJS_TZ_OK;
}

int qjs_tz_info(const QJSTimeZone *zone, int64_t seconds, QJSTzInfo *out)
{ return zone ? qjs_tzif_info(&zone->state, seconds, out) : QJS_TZ_INVALID; }
int qjs_tz_name_stable(const QJSTimeZone *zone, int64_t from, int64_t through, int *out)
{ return zone ? qjs_tzif_name_stable(&zone->state, from, through, out) : QJS_TZ_INVALID; }
