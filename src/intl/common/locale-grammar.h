/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Pure C Unicode locale grammar; no ICU or engine dependency. */
#ifndef QUICKJS_INTL_LOCALE_GRAMMAR_H
#define QUICKJS_INTL_LOCALE_GRAMMAR_H
#include <stddef.h>

/* ECMA402 Unicode BCP47 grammar, rejecting duplicate variants/singletons.
 * Unlike the UTS35 validity rules, ECMA402 permits duplicate ukey/tkey
 * fields; canonicalization later keeps the first. This validates syntax,
 * not aliases or installed locale data. The validators allocate no memory
 * and accept explicit byte lengths; NULL is invalid even for an empty slice.
 */
int qjs_intl_unicode_locale_well_formed(const char *tag, size_t length);
int qjs_intl_unicode_type_well_formed(const char *type, size_t length);

typedef enum QJSIntlLocaleSubtagKind {
    QJS_INTL_LANGUAGE_SUBTAG,
    QJS_INTL_SCRIPT_SUBTAG,
    QJS_INTL_REGION_SUBTAG,
    QJS_INTL_VARIANT_SUBTAG
} QJSIntlLocaleSubtagKind;

/* One subtag, not a hyphenated sequence. No alias or casing requirement. */
int qjs_intl_unicode_subtag_well_formed(QJSIntlLocaleSubtagKind kind,
                                      const char *text, size_t length);

#endif
