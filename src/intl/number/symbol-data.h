/* Native numeric symbol metadata; no formatter capability is implied.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_INTL_NUMBER_SYMBOL_DATA_H
#define QJS_INTL_NUMBER_SYMBOL_DATA_H
#include "../locale/numbering-data.h"

enum {
    QJS_INTL_SYMBOL_DECIMAL,
    QJS_INTL_SYMBOL_GROUP,
    QJS_INTL_SYMBOL_LIST,
    QJS_INTL_SYMBOL_PERCENT,
    QJS_INTL_SYMBOL_PLUS,
    QJS_INTL_SYMBOL_MINUS,
    QJS_INTL_SYMBOL_APPROXIMATELY,
    QJS_INTL_SYMBOL_EXPONENTIAL,
    QJS_INTL_SYMBOL_SUPERSCRIPT_EXPONENT,
    QJS_INTL_SYMBOL_PER_MILLE,
    QJS_INTL_SYMBOL_INFINITY,
    QJS_INTL_SYMBOL_NAN,
    QJS_INTL_SYMBOL_TIME_SEPARATOR,
    QJS_INTL_SYMBOL_CURRENCY_DECIMAL,
    QJS_INTL_SYMBOL_CURRENCY_GROUP,
    QJS_INTL_SYMBOL_COUNT
};
enum {
    QJS_INTL_SYMBOL_VALUE,
    QJS_INTL_SYMBOL_ABSENT,
    QJS_INTL_SYMBOL_BLOCKED
};

typedef struct QJSIntlNumberSymbol {
    QJSIntlBytes text;
    unsigned int state;
} QJSIntlNumberSymbol;

typedef struct QJSIntlNumberSymbols {
    QJSIntlNumberSymbol fields[QJS_INTL_SYMBOL_COUNT];
} QJSIntlNumberSymbols;

typedef struct QJSIntlNumberSymbolData {
    QJSIntlNumberingData numbering;
    QJSIntlSection section;
    const uint8_t *systems, *locale_rows, *rows, *vectors;
    uint32_t system_count, row_count, vector_count;
} QJSIntlNumberSymbolData;

/* Open against an unchanged, successfully validated numbering view. Borrow
   the same immutable blob; validation scratch is released before return.
   The allocator must provide malloc/free. Follow pack.h's aliasing contract;
   non-NULL outputs are zeroed on failure. No locale fallback or formatter
   availability is inferred. */
QJSIntlStatus qjs_intl_number_symbol_data_open(const QJSIntlNumberingData *numbering,
                                             const QJSIntlAllocator *allocator,
                                             QJSIntlNumberSymbolData *out);
/* Exact shared-locale/numbering-definition indexes; no allocations. An
   algorithmic definition returns UNSUPPORTED. Empty VALUE text differs from
   optional ABSENT/BLOCKED currency symbols, whose text slices are zeroed.
   Strings borrow the open view's storage; non-NULL output is zeroed on error. */
QJSIntlStatus qjs_intl_number_symbols_at(const QJSIntlNumberSymbolData *data,
                                       uint32_t locale, uint32_t system,
                                       QJSIntlNumberSymbols *out);
#endif
