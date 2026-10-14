/* Preflight only: no expected offset is obtained from the implementation.
   Copyright (c) 2026 Yan-Jie Wang. MIT license. */
#include "../../../src/timezone/timezone.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int i;
    if (argc < 2 || argc > 1025) return 2;
    for (i = 1; i < argc; i++) {
        const char *id, *primary;
        QJSTimeZone *zone;
        if (qjs_tz_resolve(argv[i], strlen(argv[i]), &id, &primary) ||
            strcmp(id, primary) || strcmp(id, argv[i]) ||
            qjs_tz_open(&zone, id, strlen(id), NULL, NULL)) {
            fprintf(stderr, "Cannot capture primary at argument %d\n", i);
            return 1;
        }
        qjs_tz_close(zone);
    }
    return 0;
}
