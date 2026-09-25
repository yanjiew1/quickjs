/*
 * QuickJS lexer interface
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
#ifndef QUICKJS_LEXER_H
#define QUICKJS_LEXER_H

#include "parse-state.h"

__exception int next_token(JSParseState *s);
__exception int js_parse_template_part(JSParseState *s, const uint8_t *p);
__exception int js_parse_string(JSParseState *s, int sep,
                                BOOL do_throw, const uint8_t *p,
                                JSToken *token, const uint8_t **pp);
__exception int js_parse_regexp(JSParseState *s);
void reparse_ident_token(JSParseState *s);
int peek_token(JSParseState *s, BOOL no_line_terminator);
void skip_shebang(const uint8_t **pp, const uint8_t *buf_end);

#endif
