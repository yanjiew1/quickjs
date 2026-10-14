/* Private, opt-in CI diagnostics. Copyright (c) 2026 Yan-Jie Wang. MIT. */
#ifndef QUICKJS_TIMEZONE_CAPTURE_H
#define QUICKJS_TIMEZONE_CAPTURE_H
#include <stddef.h>
/* Called after selection, while these exact immutable bytes remain owned
   by the new snapshot. It must not call back into the timezone provider.
   A nonzero result aborts this diagnostic constructor. Normal builds do
   not include this header, call this function, or link its implementation. */
int qjs_tz_capture_selected(const char *identifier, const char *primary,
                            int origin, int source_status,
                            const char *system_root,
                            const unsigned char *bytes, size_t size);
#endif
