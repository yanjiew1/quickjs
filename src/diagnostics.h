/*
 * QuickJS Javascript Engine
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
/* Shared engine exception construction. */
#ifndef QUICKJS_DIAGNOSTICS_H
#define QUICKJS_DIAGNOSTICS_H

#include "config.h"
#include "quickjs.h"

/* Throw the canonical stack-overflow exception. */
JS_INTERNAL JSValue JS_ThrowStackOverflow(JSContext *ctx);

#ifdef DUMP_READ_OBJECT
struct JSString;
/* Trace borrowed strings/atoms to stdout; no ownership changes. */
JS_INTERNAL void JS_DumpString(JSRuntime *rt, const struct JSString *p);
JS_INTERNAL void print_atom(JSContext *ctx, JSAtom atom);
#endif

#endif /* QUICKJS_DIAGNOSTICS_H */
