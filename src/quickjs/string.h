/*
 * QuickJS String and StringBuffer declarations
 */
#ifndef QUICKJS_STRING_H
#define QUICKJS_STRING_H

#include "quickjs/def.h"

void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);

struct JSString {
    uint32_t len : 31;
    uint8_t is_wide_char : 1; /* 0 = 8 bits, 1 = 16 bits characters */
    /* for JS_ATOM_TYPE_SYMBOL: hash = weakref_count, atom_type = 3,
       for JS_ATOM_TYPE_PRIVATE: hash = JS_ATOM_HASH_PRIVATE, atom_type = 3 */
    uint32_t hash : 30;
    uint8_t atom_type : 2; /* != 0 if atom, JS_ATOM_TYPE_x */
    uint32_t hash_next; /* atom_index for JS_ATOM_TYPE_SYMBOL */
#ifdef DUMP_LEAKS
    struct list_head link; /* string list */
#endif
    union {
        uint8_t str8[0]; /* 8 bit strings will get an extra null terminator */
        uint16_t str16[0];
    } u;
};

struct JSStringRope {
    uint32_t len;
    uint8_t is_wide_char; /* 0 = 8 bits, 1 = 16 bits characters */
    uint8_t depth; /* max depth of the rope tree */
    JSValue left;
    JSValue right; /* might be the empty string */
};

struct StringBuffer {
    JSContext *ctx;
    JSString *str;
    int len;
    int size;
    int is_wide_char;
    int error_status;
};

#define JS_VALUE_GET_STRING(v) ((JSString *)JS_VALUE_GET_PTR(v))
#define JS_VALUE_GET_STRING_ROPE(v) ((JSStringRope *)JS_VALUE_GET_PTR(v))

static inline int string_get(const JSString *p, int idx) {
    if (p->is_wide_char)
        return p->u.str16[idx];
    else
        return p->u.str8[idx];
}

static inline BOOL JS_IsEmptyString(JSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_STRING && JS_VALUE_GET_STRING(v)->len == 0;
}

JSString *js_alloc_string_rt(JSRuntime *rt, int max_len, int is_wide_char);
JSString *js_alloc_string(JSContext *ctx, int max_len, int is_wide_char);
__maybe_unused void JS_DumpString(JSRuntime *rt, const JSString *p);

static inline void js_free_string(JSRuntime *rt, JSString *str)
{
    if (--js_rc(str)->ref_count <= 0) {
        if (str->atom_type) {
            JS_FreeAtomStruct(rt, (JSAtomStruct *)str);
        } else {
#ifdef DUMP_LEAKS
            list_del(&str->link);
#endif
            js_free_rt(rt, str);
        }
    }
}

int string_buffer_init2(JSContext *ctx, StringBuffer *s, int size, int is_wide);
static inline int string_buffer_init(JSContext *ctx, StringBuffer *s, int size)
{
    return string_buffer_init2(ctx, s, size, 0);
}
void string_buffer_free(StringBuffer *s);
int string_buffer_set_error(StringBuffer *s);
int string_buffer_widen(StringBuffer *s, int size);
int string_buffer_realloc(StringBuffer *s, int new_len, int c);
int string_buffer_putc16_slow(StringBuffer *s, uint32_t c);
int string_buffer_putc8(StringBuffer *s, uint32_t c);
int string_buffer_putc16(StringBuffer *s, uint32_t c);
int string_buffer_putc_slow(StringBuffer *s, uint32_t c);
static inline int string_buffer_putc(StringBuffer *s, uint32_t c)
{
    if (likely(s->len < s->size)) {
        if (s->is_wide_char) {
            if (c < 0x10000) {
                s->str->u.str16[s->len++] = c;
                return 0;
            } else if (likely((s->len + 1) < s->size)) {
                s->str->u.str16[s->len++] = get_hi_surrogate(c);
                s->str->u.str16[s->len++] = get_lo_surrogate(c);
                return 0;
            }
        } else if (c < 0x100) {
            s->str->u.str8[s->len++] = c;
            return 0;
        }
    }
    return string_buffer_putc_slow(s, c);
}
int string_buffer_write8(StringBuffer *s, const uint8_t *p, int len);
int string_buffer_write16(StringBuffer *s, const uint16_t *p, int len);
int string_buffer_puts8(StringBuffer *s, const char *str);
int string_buffer_concat(StringBuffer *s, const JSString *p, uint32_t from, uint32_t to);
int string_buffer_concat_value(StringBuffer *s, JSValueConst v);
int string_buffer_concat_value_free(StringBuffer *s, JSValue v);
int string_buffer_fill(StringBuffer *s, int c, int count);
JSValue string_buffer_end(StringBuffer *s);

static inline int js_string_find_invalid_codepoint(JSString *p)
{
    int i;
    if (!p->is_wide_char)
        return -1;
    for(i = 0; i < p->len; i++) {
        uint32_t c = p->u.str16[i];
        if (is_surrogate(c)) {
            if (is_hi_surrogate(c) && (i + 1) < p->len
            &&  is_lo_surrogate(p->u.str16[i + 1])) {
                i++;
            } else {
                return i;
            }
        }
    }
    return -1;
}

JSValue JS_ConcatString(JSContext *ctx, JSValue op1, JSValue op2);
JSValue JS_ConcatString3(JSContext *ctx, const char *str1, JSValue str2, const char *str3);
JSValue js_new_string8(JSContext *ctx, const char *buf);
JSValue js_new_string8_len(JSContext *ctx, const char *buf, int len);
JSValue js_new_string16_len(JSContext *ctx, const uint16_t *buf, int len);
JSValue js_new_string_char(JSContext *ctx, uint16_t c);
JSValue js_sub_string(JSContext *ctx, JSString *p, int start, int end);
int js_string_compare(JSContext *ctx, const JSString *p1, const JSString *p2);
int js_string_memcmp(const JSString *p1, int pos1, const JSString *p2, int pos2, int len);
BOOL js_string_eq(JSContext *ctx, const JSString *p1, const JSString *p2);
int js_string_rope_compare(JSContext *ctx, JSValueConst op1, JSValueConst op2, BOOL is_strict);
JSValue js_linearize_string_rope(JSContext *ctx, JSValue rope);
BOOL JS_ConcatStringInPlace(JSContext *ctx, JSString *p1, JSValueConst op2);
int string_rope_get(JSValueConst val, uint32_t idx);
uint32_t js_string_obj_get_length(JSContext *ctx, JSValueConst obj);
int string_getc(const JSString *p, int *pidx);
uint32_t hash_string(const JSString *str, uint32_t h);
uint32_t hash_string_rope(JSValueConst val, uint32_t h);
int64_t string_advance_index(JSString *p, int64_t index, BOOL unicode);
int string_indexof_char(JSString *p, int c, int from);
int js_string_GetSubstitution(JSContext *ctx,
                              StringBuffer *b,
                              JSValueConst matched,
                              JSString *sp,
                              uint32_t position,
                              JSValueConst captures_val,
                              JSValueConst namedCaptures,
                              JSValueConst rep,
                              uint8_t **captures,
                              uint32_t captures_len);
int skip_spaces(const char *pc);


#endif /* QUICKJS_STRING_H */
