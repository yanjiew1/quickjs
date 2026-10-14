/* Source-only witness: both private include orders must be legal C99/C++11. */
#if HEADER_ORDER == 1
#include "intl/provider.h"
#include "timezone/timezone.h"
#include "temporal/time-zone.h"
#else
#include "temporal/time-zone.h"
#include "timezone/timezone.h"
#include "intl/provider.h"
#endif

QJSTzProvider *qjs_timezone_alias_owner_witness(struct QJSTzProvider *value)
{
    return value;
}
