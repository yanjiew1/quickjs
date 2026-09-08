/*
 * QuickJS Javascript Engine: Atom and Symbol Definitions
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

#ifndef QUICKJS_VALUE_JS_ATOM_H
#define QUICKJS_VALUE_JS_ATOM_H

#include "quickjs-internal.h"

/* Atom allocation and helpers */
static inline uint32_t atom_get_free(const JSAtomStruct *p)
{
    return (uintptr_t)p >> 1;
}

static inline BOOL atom_is_free(const JSAtomStruct *p)
{
    return (uintptr_t)p & 1;
}

static inline JSAtomStruct *atom_set_free(uint32_t v)
{
    return (JSAtomStruct *)(((uintptr_t)v << 1) | 1);
}

static inline BOOL __JS_AtomIsConst(JSAtom v)
{
#if defined(DUMP_LEAKS) && DUMP_LEAKS > 1
    return (int32_t)v <= 0;
#else
    return (int32_t)v < JS_ATOM_END;
#endif
}

/* Atom Lifecycle & Hash Table */
int JS_InitAtoms(JSRuntime *rt);
JSAtom JS_DupAtomRT(JSRuntime *rt, JSAtom v);
JSAtom JS_DupAtom(JSContext *ctx, JSAtom v);
void JS_FreeAtomRT(JSRuntime *rt, JSAtom v);
void JS_FreeAtom(JSContext *ctx, JSAtom v);
void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);

/* Atom Query & Introspection */
JSAtomKindEnum JS_AtomGetKind(JSContext *ctx, JSAtom v);
BOOL JS_AtomIsString(JSContext *ctx, JSAtom v);
JSAtom js_get_atom_index(JSRuntime *rt, JSAtomStruct *p);
BOOL JS_AtomIsArrayIndex(JSContext *ctx, uint32_t *pval, JSAtom atom);
JSValue JS_AtomIsNumericIndex1(JSContext *ctx, JSAtom atom);
int JS_AtomIsNumericIndex(JSContext *ctx, JSAtom atom);
BOOL JS_AtomSymbolHasDescription(JSContext *ctx, JSAtom v);

/* Atom Creation */
JSAtom __JS_NewAtomInit(JSRuntime *rt, const char *str, int len, int atom_type);
JSAtom __JS_FindAtom(JSRuntime *rt, const char *str, size_t len, int atom_type);
JSAtom JS_NewAtomStr(JSContext *ctx, JSString *p);
JSAtom JS_NewAtomLen(JSContext *ctx, const char *str, size_t len);
JSAtom JS_NewAtom(JSContext *ctx, const char *str);
JSAtom JS_NewAtomUInt32(JSContext *ctx, uint32_t n);
JSAtom JS_NewAtomInt64(JSContext *ctx, int64_t n);

/* Symbols */
JSValue JS_NewSymbol(JSContext *ctx, JSString *p, int atom_type);
JSValue JS_NewSymbolFromAtom(JSContext *ctx, JSAtom descr, int atom_type);
JSAtom js_symbol_to_atom(JSContext *ctx, JSValue val);

/* Atom Conversion */
JSValue JS_AtomToValue(JSContext *ctx, JSAtom atom);
JSValue JS_AtomToString(JSContext *ctx, JSAtom atom);
JSAtom JS_ValueToAtom(JSContext *ctx, JSValueConst val);
const char *JS_AtomGetStrRT(JSRuntime *rt, char *buf, int buf_size, JSAtom atom);
const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom);

/* Atom Concatenation */
JSAtom js_atom_concat_str(JSContext *ctx, JSAtom name, const char *str1);
JSAtom js_atom_concat_num(JSContext *ctx, JSAtom name, uint32_t n);

/* Debug Dumps */
void JS_DumpChar(FILE *fo, int c, int sep);
void JS_DumpString(JSRuntime *rt, const JSString *p);
void JS_DumpAtoms(JSRuntime *rt);

#endif /* QUICKJS_VALUE_JS_ATOM_H */
