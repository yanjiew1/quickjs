/* Pending shared runtime timezone integration; one provider per runtime. */
#ifndef QUICKJS_INTL_NATIVE_DATE_TIMEZONE_CONTRACT_H
#define QUICKJS_INTL_NATIVE_DATE_TIMEZONE_CONTRACT_H
#include "../../internal/base.h"
#include "../../../timezone/timezone.h"
/* Shared Date/Temporal timezone owner must supply this accessor before linking.
 * It creates/borrows the system-first embedded provider using runtime allocation,
 * throws on failure, and outlives all DateTimeFormat banks. No JSContext borrowed
 * by a bank. Default-zone snapshots remain in the shared Intl context owner. */
QJSTzProvider *js_intl_native_time_zone_provider(JSContext *);
#endif
