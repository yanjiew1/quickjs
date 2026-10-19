/* Native locale-name composition, independent of JS values and API fallback.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DISPLAY_LANGUAGE_FORMAT_H
#define QJS_INTL_DISPLAY_LANGUAGE_FORMAT_H
#include "language-match.h"

/* Input follows language-match.h's validated canonical Language ID contract.
   Compose unconsumed script/region/variants using source patterns and nested
   bracket replacements. Missing qualifiers use their canonical input subtags;
   a missing localized base returns OK/absent independently of API fallback.
   Require malloc/free and a nonzero UTF-16 limit. Allocate no intermediates
   proportional to rendered output; release all scratch on success or failure.
   Result owns its text with the supplied allocator and borrows nothing.
   Require disjoint output; reset non-NULL output on failure. */
QJSIntlStatus qjs_intl_language_name_format(const QJSIntlNameData *, uint32_t locale,
                                          QJSIntlNameStyle, int dialect,
                                          QJSIntlBytes canonical,
                                          const QJSIntlAllocator *, size_t limit,
                                          QJSIntlNameText *);
#endif
