/* Exact selected TZif bytes for an isolated Linux diagnostic build.
   Copyright (c) 2026 Yan-Jie Wang. MIT license. */
#ifdef _WIN32
#error This capture transport is only enabled in the Linux diagnostic row.
#endif
#include "../../../src/timezone/capture.h"
#include "../../../src/timezone/timezone.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The root launcher owns a new, empty directory for each child. The
   manifest contains no offsets: an independent decoder derives them. */
int qjs_tz_capture_selected(const char *identifier, const char *primary,
                            int origin, int source_status,
                            const char *system_root,
                            const unsigned char *bytes, size_t size)
{
    struct Seen { char primary[128]; int origin, status; size_t size; };
    static struct Seen seen[1024];
    static char original_directory[2049], original_root[2049];
    static unsigned count;
    const char *directory = getenv("QJS_TZ_CAPTURE_DIRECTORY");
    const char *strings[3] = {identifier, primary, system_root};
    char path[4096], blob[32];
    FILE *file;
    int failed, n;
    unsigned i;
    if (!directory) return 0;
    if (directory[0] != '/' || strlen(directory) > 2048 || !primary ||
        !system_root || !bytes ||
        size < 44 || size > 65536 || strlen(primary) >= 128 ||
        strlen(system_root) > 2048 ||
        (origin != QJS_TZ_SYSTEM && origin != QJS_TZ_EMBEDDED) ||
        source_status < QJS_TZ_OK || source_status > QJS_TZ_RANGE)
        return -1;
    /* Generated labels and the explicitly bound build directory contain
       only these ASCII bytes, so JSON escaping cannot be ambiguous. */
    for (i = 0; i < 3; i++) {
        const unsigned char *p = (const unsigned char *)strings[i];
        if (!p || !*p) return -1;
        for (; *p; p++)
            if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                  (*p >= '0' && *p <= '9') || strchr("/_+.-", *p)))
                return -1;
    }
    if (count && (strcmp(directory, original_directory) ||
                  strcmp(system_root, original_root))) return -1;
    for (i = 0; i < count; i++) {
        if (!strcmp(primary, seen[i].primary)) {
            unsigned char copy[4096];
            size_t position = 0;
            if (origin != seen[i].origin || source_status != seen[i].status ||
                size != seen[i].size) return -1;
            n = snprintf(path, sizeof path, "%s/snapshot-%04u.tzif", directory, i + 1);
            if (n < 0 || n >= (int)sizeof path) return -1;
            file = fopen(path, "rb");
            if (!file) return -1;
            failed = 0;
            while (position < size) {
                size_t amount = size - position;
                if (amount > sizeof copy) amount = sizeof copy;
                if (fread(copy, 1, amount, file) != amount ||
                    memcmp(copy, bytes + position, amount)) { failed = 1; break; }
                position += amount;
            }
            if (!failed && fgetc(file) != EOF) failed = 1;
            if (ferror(file)) failed = 1;
            if (fclose(file)) failed = 1;
            return failed ? -1 : 0;
        }
    }
    if (count >= 1024) return -1;
    n = snprintf(blob, sizeof blob, "snapshot-%04u.tzif", count + 1);
    if (n < 0 || n >= (int)sizeof blob) return -1;
    n = snprintf(path, sizeof path, "%s/%s", directory, blob);
    if (n < 0 || n >= (int)sizeof path) return -1;
    file = fopen(path, "wbx");
    if (!file) return -1;
    failed = fwrite(bytes, 1, size, file) != size;
    if (fclose(file)) failed = 1;
    if (failed) return -1;
    n = snprintf(path, sizeof path, "%s/selected-timezones.ndjson", directory);
    if (n < 0 || n >= (int)sizeof path) return -1;
    file = fopen(path, count ? "a" : "wx");
    if (!file) return -1;
    failed = fprintf(file,
        "{\"schema\":\"qjs-selected-tzif-record-v1\",\"pid\":%ld,"
        "\"sequence\":%u,\"identifier\":\"%s\",\"primary\":\"%s\","
        "\"origin\":\"%s\",\"source_status\":%d,"
        "\"system_root\":\"%s\",\"blob\":\"%s\",\"size\":%zu}\n",
        (long)getpid(), count + 1, identifier, primary,
        origin == QJS_TZ_SYSTEM ? "system" : "embedded", source_status,
        system_root, blob, size) < 0;
    if (fclose(file)) failed = 1;
    if (failed) return -1;
    if (!count) {
        strcpy(original_directory, directory);
        strcpy(original_root, system_root);
    }
    strcpy(seen[count].primary, primary);
    seen[count].origin = origin;
    seen[count].status = source_status;
    seen[count].size = size;
    count++;
    return 0;
}
