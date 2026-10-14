#include "quickjs.h"
static int (*set_default_time_zone)(JSContext *, const char *) = JS_SetDefaultTimeZone;
static JSValue (*get_default_time_zone)(JSContext *) = JS_GetDefaultTimeZone;
int main(void) { return set_default_time_zone == 0 || get_default_time_zone == 0; }
