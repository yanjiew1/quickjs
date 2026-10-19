/* Owned text construction shared by native display-name adapters.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DISPLAY_TEXT_H
#define QJS_INTL_DISPLAY_TEXT_H
#include "display.h"

/* Copy one present scalar UTF-8 label to bounded owned UTF-16. Empty is present;
   missing labels must be handled by the caller without invoking this operation.
   Require malloc/free, nonzero limit and disjoint output; reset on failure. */
QJSIntlStatus qjs_intl_name_text_copy(const QJSIntlAllocator *, QJSIntlBytes,
                                      size_t limit, QJSIntlNameText *);
#endif
