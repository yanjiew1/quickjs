/*
 * QuickJS Lexer Internal Interface
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
#ifndef QJS_LEXER_H
#define QJS_LEXER_H

#include "parser.h"

QJS_INTERNAL __attribute__((format(printf, 2, 3)))
int js_parse_error(JSParseState *s, const char *fmt, ...);
QJS_INTERNAL __exception int json_next_token(JSParseState *s);
QJS_INTERNAL void free_token(JSParseState *s, JSToken *token);
QJS_INTERNAL int get_line_col_cached(GetLineColCache *s, int *pcol_num,
                                      const uint8_t *ptr);

static inline BOOL token_is_pseudo_keyword(JSParseState *s, JSAtom atom) {
    return s->token.val == TOK_IDENT && s->token.u.ident.atom == atom &&
        !s->token.u.ident.has_escape;
}


QJS_INTERNAL int get_line_col(int *pcol_num, const uint8_t *buf, size_t len);
QJS_INTERNAL __attribute__((format(printf, 3, 4))) int js_parse_error_pos(JSParseState *s, const uint8_t *ptr, const char *fmt, ...);
QJS_INTERNAL int js_parse_error_reserved_identifier(JSParseState *s);
QJS_INTERNAL int js_parse_expect(JSParseState *s, int tok);
QJS_INTERNAL int js_parse_expect_semi(JSParseState *s);
QJS_INTERNAL __exception int js_parse_regexp(JSParseState *s);
QJS_INTERNAL __exception int js_parse_string(JSParseState *s, int sep,
                                       BOOL do_throw, const uint8_t *p,
                                       JSToken *token, const uint8_t **pp);
QJS_INTERNAL __exception int js_parse_template_part(JSParseState *s, const uint8_t *p);
QJS_INTERNAL __exception int next_token(JSParseState *s);
QJS_INTERNAL int peek_token(JSParseState *s, BOOL no_line_terminator);
QJS_INTERNAL void reparse_ident_token(JSParseState *s);
QJS_INTERNAL void skip_shebang(const uint8_t **pp, const uint8_t *buf_end);

#endif /* QJS_LEXER_H */
