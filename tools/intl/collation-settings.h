/* Host interpretation of scalar rule settings, separate from weight mutation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMPILER_SETTINGS_H
#define QJS_INTL_COMPILER_SETTINGS_H
#include "collation-ir.h"
#include "../../src/intl/collation/policy.h"

/* Root defaults. Reordering has a separate group owner. Recording normalization
   off permits an optimized FCD path; it does not justify breaking canonical
   equivalence. maxVariable applies after construction, not to logical resets. */
void qjs_intl_collation_settings_init(QJSIntlCollationPolicy *);
/* Apply a validated setting instruction to an initialized policy. No allocation; policy is unchanged on
   failure. Known scalar values are checked exactly. Reorder and unsupported
   legacy settings return UNSUPPORTED for their owning executor to handle or
   reject. Input slices and output policy are disjoint. */
QJSIntlStatus qjs_intl_collation_setting_apply(QJSIntlCollationPolicy *, const QJSIntlIRInstruction *);
#endif
