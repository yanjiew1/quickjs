/* Final collation ranks are distinct from root ordinals and construction IDs.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_RANKED_H
#define QJS_INTL_COLLATION_RANKED_H
#include "../common/types.h"

typedef struct QJSIntlCollationRankedElement {
    uint32_t ranks[4]; /* Full uint32 ranks; bit31 is not a tailored-ID flag. */
    uint32_t root_primary;
    unsigned char case_value, variable, implicit; /* variable is the root default. */
} QJSIntlCollationRankedElement;
#endif
