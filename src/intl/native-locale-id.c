/* Native locale syntax, aliases and likely subtags, using schema 1.0.
 * Pure C implementation; frontend coercions/options remain frontend-owned.
 * Syntax/ownership helpers adapted from the reviewed locale-syntax.c.
 */
#include "intl/native-locale-id.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct LocaleContext {
    const QJSIntlAllocator *allocator;
    const QJSIntlDataView *view;
    QJSIntlDataSection aliases, replacements, likely, bcp;
    QJSIntlStatus status;
} LocaleContext;
typedef struct LocaleList { char **items; size_t count; } LocaleList;
typedef struct IntlLanguageId {
    char *language, *script, *region;
    LocaleList variants;
} IntlLanguageId;
typedef struct IntlKeyword { char *key, *value; } IntlKeyword;
typedef struct IntlExtension {
    char singleton;
    IntlLanguageId tlang;
    LocaleList attributes;
    IntlKeyword *keywords;
    size_t count;
    char *other;
} IntlExtension;
typedef struct IntlTag {
    IntlLanguageId base;
    IntlExtension *extensions;
    size_t count;
    char *private_use;
} IntlTag;
static void *ctx_malloc(LocaleContext *ctx, size_t size)
{
    void *p = ctx->allocator->malloc(ctx->allocator->opaque, size);
    if (!p) ctx->status = QJS_INTL_NO_MEMORY;
    return p;
}
static void *ctx_realloc(LocaleContext *ctx, void *old, size_t size)
{
    void *p = ctx->allocator->realloc(ctx->allocator->opaque, old, size);
    if (!p) ctx->status = QJS_INTL_NO_MEMORY;
    return p;
}
static void ctx_free(LocaleContext *ctx, void *p)
{
    if (p) ctx->allocator->free(ctx->allocator->opaque, p);
}
static char *ctx_strdup(LocaleContext *ctx, const char *s)
{
    size_t n = strlen(s);
    char *p;
    if (n == SIZE_MAX) { ctx->status = QJS_INTL_OVERFLOW; return NULL; }
    p = ctx_malloc(ctx, n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
static void list_free(LocaleContext *ctx, LocaleList *list)
{
    size_t i;
    for (i = 0; i < list->count; i++) ctx_free(ctx, list->items[i]);
    ctx_free(ctx, list->items); memset(list, 0, sizeof(*list));
}
static int list_append(LocaleContext *ctx, LocaleList *list, const char *s)
{
    char *value, **items;
    if (list->count >= SIZE_MAX / sizeof(*items)) {
        ctx->status = QJS_INTL_OVERFLOW; return -1;
    }
    value = ctx_strdup(ctx, s);
    if (!value) return -1;
    items = ctx_realloc(ctx, list->items, (list->count + 1) * sizeof(*items));
    if (!items) { ctx_free(ctx, value); return -1; }
    list->items = items; items[list->count++] = value; return 0;
}
typedef struct IntlBuffer { char *data; size_t size, capacity; } IntlBuffer;
static int ascii_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int ascii_digit(int c) { return c >= '0' && c <= '9'; }
static int ascii_alnum(int c) { return ascii_alpha(c) || ascii_digit(c); }
static int ascii_lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int all_chars(const char *s, int (*test)(int))
{
    for (; *s; s++) if (!test((unsigned char)*s)) return 0;
    return 1;
}
static int language_subtag(const char *s)
{
    size_t n = strlen(s);
    return (n == 2 || n == 3 || (n >= 5 && n <= 8)) && all_chars(s, ascii_alpha);
}
static int script_subtag(const char *s) { return strlen(s) == 4 && all_chars(s, ascii_alpha); }
static int region_subtag(const char *s)
{
    return (strlen(s) == 2 && all_chars(s, ascii_alpha)) ||
           (strlen(s) == 3 && all_chars(s, ascii_digit));
}
static int variant_subtag(const char *s)
{
    size_t n = strlen(s);
    return ((n >= 5 && n <= 8) || (n == 4 && ascii_digit(*s))) && all_chars(s, ascii_alnum);
}
static char *copy_n(LocaleContext *ctx, const char *s, size_t n)
{
    char *r;
    if (n == SIZE_MAX) { ctx->status = QJS_INTL_OVERFLOW; return NULL; }
    r = ctx_malloc(ctx, n + 1);
    if (r) { memcpy(r, s, n); r[n] = 0; }
    return r;
}
static int buffer_add(LocaleContext *ctx, IntlBuffer *b, const char *s)
{
    size_t n = strlen(s), need, cap;
    char *r;
    if (n > SIZE_MAX - b->size - 1) { ctx->status = QJS_INTL_OVERFLOW; return -1; }
    need = b->size + n + 1;
    if (need > b->capacity) {
        cap = b->capacity > SIZE_MAX / 2 ? need : b->capacity * 2;
        if (cap < need) cap = need;
        r = ctx_realloc(ctx, b->data, cap);
        if (!r) return -1;
        b->data = r; b->capacity = cap;
    }
    memcpy(b->data + b->size, s, n + 1); b->size += n;
    return 0;
}
static int buffer_subtag(LocaleContext *ctx, IntlBuffer *b, const char *s)
{
    return (b->size && buffer_add(ctx, b, "-") < 0) || buffer_add(ctx, b, s) < 0 ? -1 : 0;
}
static int string_compare(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}
static int list_contains(const LocaleList *list, const char *s)
{
    size_t i;
    for (i = 0; i < list->count; i++) if (!strcmp(list->items[i], s)) return 1;
    return 0;
}
static int intl_list_sort_unique(LocaleContext *ctx, LocaleList *list)
{
    size_t i, out = 0;
    if (list->count > 1)
        qsort(list->items, list->count, sizeof(*list->items), string_compare);
    for (i = 0; i < list->count; i++) {
        if (out && !strcmp(list->items[out - 1], list->items[i])) ctx_free(ctx, list->items[i]);
        else list->items[out++] = list->items[i];
    }
    list->count = out;
    return 0;
}
static void language_free(LocaleContext *ctx, IntlLanguageId *id)
{
    ctx_free(ctx, id->language); ctx_free(ctx, id->script); ctx_free(ctx, id->region);
    list_free(ctx, &id->variants); memset(id, 0, sizeof(*id));
}
static void intl_tag_free(LocaleContext *ctx, IntlTag *tag)
{
    size_t i, j;
    language_free(ctx, &tag->base);
    for (i = 0; i < tag->count; i++) {
        IntlExtension *e = &tag->extensions[i];
        language_free(ctx, &e->tlang);
        list_free(ctx, &e->attributes);
        for (j = 0; j < e->count; j++) { ctx_free(ctx, e->keywords[j].key); ctx_free(ctx, e->keywords[j].value); }
        ctx_free(ctx, e->keywords); ctx_free(ctx, e->other);
    }
    ctx_free(ctx, tag->extensions); ctx_free(ctx, tag->private_use);
    memset(tag, 0, sizeof(*tag));
}
static int parse_language(LocaleContext *ctx, char **tokens, size_t end,
                          size_t *position, IntlLanguageId *id)
{
    size_t p = *position;
    if (p == end || !language_subtag(tokens[p])) return 1;
    if (!(id->language = ctx_strdup(ctx, tokens[p++]))) return -1;
    if (p < end && script_subtag(tokens[p])) {
        if (!(id->script = ctx_strdup(ctx, tokens[p++]))) return -1;
    }
    if (p < end && region_subtag(tokens[p])) {
        if (!(id->region = ctx_strdup(ctx, tokens[p++]))) return -1;
    }
    while (p < end && variant_subtag(tokens[p])) {
        if (list_contains(&id->variants, tokens[p])) return 1;
        if (list_append(ctx, &id->variants, tokens[p++]) < 0) return -1;
    }
    *position = p;
    return 0;
}
static char *join_tokens(LocaleContext *ctx, char **tokens, size_t p, size_t end)
{
    IntlBuffer b = { 0 };
    for (; p < end; p++) if (buffer_subtag(ctx, &b, tokens[p]) < 0) { ctx_free(ctx, b.data); return NULL; }
    if (!b.data) return ctx_strdup(ctx, "");
    return b.data;
}
static int keyword_set(LocaleContext *ctx, IntlExtension *e, const char *key,
                       const char *value, int override)
{
    size_t i;
    IntlKeyword *r;
    char *k, *v;
    for (i = 0; i < e->count; i++) if (!strcmp(e->keywords[i].key, key)) {
        if (!override) return 0;
        v = ctx_strdup(ctx, value); if (!v) return -1;
        ctx_free(ctx, e->keywords[i].value); e->keywords[i].value = v; return 0;
    }
    if (e->count >= SIZE_MAX / sizeof(*r)) { ctx->status = QJS_INTL_OVERFLOW; return -1; }
    k = ctx_strdup(ctx, key); v = ctx_strdup(ctx, value);
    if (!k || !v) { ctx_free(ctx, k); ctx_free(ctx, v); return -1; }
    r = ctx_realloc(ctx, e->keywords, (e->count + 1) * sizeof(*r));
    if (!r) { ctx_free(ctx, k); ctx_free(ctx, v); return -1; }
    e->keywords = r; r[e->count++] = (IntlKeyword){ k, v }; return 0;
}
static IntlExtension *extension_add(LocaleContext *ctx, IntlTag *tag, char singleton)
{
    IntlExtension *r;
    if (tag->count >= SIZE_MAX / sizeof(*r)) { ctx->status = QJS_INTL_OVERFLOW; return NULL; }
    r = ctx_realloc(ctx, tag->extensions, (tag->count + 1) * sizeof(*r));
    if (!r) return NULL;
    tag->extensions = r; r += tag->count++; memset(r, 0, sizeof(*r)); r->singleton = singleton; return r;
}
static int intl_parse_tag(LocaleContext *ctx, const char *tag, size_t length, IntlTag *out)
{
    char *storage = NULL, **tokens = NULL;
    size_t count = 1, i, p, end, start;
    unsigned char seen[128] = { 0 };
    int r;
    memset(out, 0, sizeof(*out));
    if (length == SIZE_MAX) { ctx->status = QJS_INTL_OVERFLOW; return -1; }
    if (!tag || !length) goto invalid;
    for (i = 0; i < length; i++) {
        if (tag[i] == '-') count++;
        else if (!ascii_alnum((unsigned char)tag[i])) goto invalid;
    }
    if (count > SIZE_MAX / sizeof(*tokens)) goto overflow;
    storage = copy_n(ctx, tag, length); tokens = ctx_malloc(ctx, count * sizeof(*tokens));
    if (!storage || !tokens) goto fail;
    for (i = 0; i < length; i++) storage[i] = ascii_lower(storage[i]);
    tokens[0] = storage; p = 1;
    for (i = 0; i < length; i++) if (storage[i] == '-') { storage[i] = 0; tokens[p++] = storage + i + 1; }
    for (i = 0; i < count; i++) if (!*tokens[i] || strlen(tokens[i]) > 8) goto invalid;
    p = 0; r = parse_language(ctx, tokens, count, &p, &out->base);
    if (r < 0) goto fail;
    if (r > 0) goto invalid;
    while (p < count) {
        IntlExtension *e;
        char singleton;
        if (strlen(tokens[p]) != 1) goto invalid;
        singleton = tokens[p++][0];
        if (singleton == 'x') {
            if (p == count) goto invalid;
            out->private_use = join_tokens(ctx, tokens, p, count);
            if (!out->private_use) goto fail;
            p = count; break;
        }
        if (seen[(unsigned char)singleton]) goto invalid;
        seen[(unsigned char)singleton] = 1;
        end = p; while (end < count && strlen(tokens[end]) > 1) end++;
        if (end == p) goto invalid;
        e = extension_add(ctx, out, singleton); if (!e) goto fail;
        if (singleton == 'u') {
            while (p < end && strlen(tokens[p]) >= 3) {
                if (!list_contains(&e->attributes, tokens[p]) && list_append(ctx, &e->attributes, tokens[p]) < 0) goto fail;
                p++;
            }
            while (p < end) {
                const char *key = tokens[p++]; char *value;
                if (strlen(key) != 2 || !ascii_alpha(key[1])) goto invalid;
                start = p; while (p < end && strlen(tokens[p]) >= 3) p++;
                value = join_tokens(ctx, tokens, start, p); if (!value) goto fail;
                r = keyword_set(ctx, e, key, value, 0); ctx_free(ctx, value); if (r < 0) goto fail;
            }
        } else if (singleton == 't') {
            if (language_subtag(tokens[p])) {
                r = parse_language(ctx, tokens, end, &p, &e->tlang);
                if (r < 0) goto fail;
                if (r > 0) goto invalid;
            }
            while (p < end) {
                const char *key = tokens[p++]; char *value;
                if (strlen(key) != 2 || !ascii_alpha(key[0]) || !ascii_digit(key[1])) goto invalid;
                start = p; while (p < end && strlen(tokens[p]) >= 3) p++;
                if (p == start) goto invalid;
                value = join_tokens(ctx, tokens, start, p); if (!value) goto fail;
                r = keyword_set(ctx, e, key, value, 0); ctx_free(ctx, value); if (r < 0) goto fail;
            }
        } else {
            e->other = join_tokens(ctx, tokens, p, end); if (!e->other) goto fail; p = end;
        }
    }
    ctx_free(ctx, storage); ctx_free(ctx, tokens); return 0;
overflow:
    ctx->status = QJS_INTL_OVERFLOW; goto fail;
invalid:
    (ctx->status = QJS_INTL_INVALID_ARGUMENT);
fail:
    ctx_free(ctx, storage); ctx_free(ctx, tokens); intl_tag_free(ctx, out); return -1;
}
static int emit_language(LocaleContext *ctx, IntlBuffer *b, const IntlLanguageId *id, int lower)
{
    char field[9]; size_t i, j;
    if (buffer_subtag(ctx, b, id->language) < 0) return -1;
    if (id->script) {
        strcpy(field, id->script); if (!lower) field[0] -= 32;
        if (buffer_subtag(ctx, b, field) < 0) return -1;
    }
    if (id->region) {
        strcpy(field, id->region);
        if (!lower) for (j = 0; field[j]; j++) if (ascii_alpha(field[j])) field[j] -= 32;
        if (buffer_subtag(ctx, b, field) < 0) return -1;
    }
    for (i = 0; i < id->variants.count; i++) if (buffer_subtag(ctx, b, id->variants.items[i]) < 0) return -1;
    return 0;
}
static char *intl_language_string(LocaleContext *ctx, const IntlLanguageId *id)
{
    IntlBuffer b = { 0 };
    if (emit_language(ctx, &b, id, 0) < 0) { ctx_free(ctx, b.data); return NULL; }
    return b.data;
}
static int extension_compare(const void *a, const void *b)
{
    return ((const IntlExtension *)a)->singleton - ((const IntlExtension *)b)->singleton;
}
static int keyword_compare(const void *a, const void *b)
{
    return strcmp(((const IntlKeyword *)a)->key, ((const IntlKeyword *)b)->key);
}
static char *intl_tag_string(LocaleContext *ctx, const IntlTag *tag)
{
    IntlBuffer b = { 0 }; size_t i, j;
    if (emit_language(ctx, &b, &tag->base, 0) < 0) goto fail;
    for (i = 0; i < tag->count; i++) {
        const IntlExtension *e = &tag->extensions[i]; char singleton[2] = { e->singleton, 0 };
        if (buffer_subtag(ctx, &b, singleton) < 0) goto fail;
        if (e->tlang.language && emit_language(ctx, &b, &e->tlang, 1) < 0) goto fail;
        for (j = 0; j < e->attributes.count; j++) if (buffer_subtag(ctx, &b, e->attributes.items[j]) < 0) goto fail;
        for (j = 0; j < e->count; j++) {
            if (buffer_subtag(ctx, &b, e->keywords[j].key) < 0) goto fail;
            if (*e->keywords[j].value && buffer_subtag(ctx, &b, e->keywords[j].value) < 0) goto fail;
        }
        if (e->other && buffer_subtag(ctx, &b, e->other) < 0) goto fail;
    }
    if (tag->private_use && (buffer_subtag(ctx, &b, "x") < 0 || buffer_subtag(ctx, &b, tag->private_use) < 0)) goto fail;
    return b.data;
fail:
    ctx_free(ctx, b.data); return NULL;
}
static int context_init(LocaleContext *ctx, const QJSIntlAllocator *allocator,
                        const QJSIntlDataView *view, int metadata)
{
    memset(ctx, 0, sizeof(*ctx));
    if (!allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
        return -1;
    ctx->allocator = allocator; ctx->view = view;
    if (!metadata) return 0;
    if (!view) return -1;
    if (qjs_intl_data_section(view, QJS_INTL_DATA_ALIAS, &ctx->aliases) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_REPLACEMENT_LIST, &ctx->replacements) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LIKELY, &ctx->likely) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_BCP47, &ctx->bcp) != QJS_INTL_DATA_OK) {
        ctx->status = QJS_INTL_DATA_ERROR; return -1;
    }
    return 0;
}
static uint32_t record_number(LocaleContext *ctx, const QJSIntlDataSection *s,
                              uint32_t index, uint32_t offset)
{
    uint32_t value = 0;
    if (qjs_intl_data_record_u32(s, index, offset, &value) != QJS_INTL_DATA_OK)
        ctx->status = QJS_INTL_DATA_ERROR;
    return value;
}
static QJSIntlDataSlice record_text(LocaleContext *ctx, const QJSIntlDataSection *s,
                                   uint32_t index, uint32_t offset)
{
    QJSIntlDataSlice value = { 0 };
    if (qjs_intl_data_record_string(ctx->view, s, index, offset, &value) != QJS_INTL_DATA_OK)
        ctx->status = QJS_INTL_DATA_ERROR;
    return value;
}
static int slice_equal(QJSIntlDataSlice s, const char *text)
{
    size_t i;
    if (s.length != strlen(text)) return 0;
    for (i = 0; i < s.length; i++)
        if (ascii_lower(s.data[i]) != ascii_lower((unsigned char)text[i])) return 0;
    return 1;
}
static char *slice_copy(LocaleContext *ctx, QJSIntlDataSlice s)
{
    char *p; size_t i;
    if (ctx->status != QJS_INTL_OK) return NULL;
    if (s.length == SIZE_MAX) { ctx->status = QJS_INTL_OVERFLOW; return NULL; }
    p = ctx_malloc(ctx, s.length + 1);
    if (p) {
        for (i = 0; i < s.length; i++) p[i] = ascii_lower(s.data[i]);
        p[s.length] = 0;
    }
    return p;
}
static QJSIntlDataSlice replacement(LocaleContext *ctx, uint32_t alias, uint32_t choice)
{
    uint32_t first = record_number(ctx, &ctx->aliases, alias, 12);
    uint32_t count = record_number(ctx, &ctx->aliases, alias, 16);
    QJSIntlDataSlice empty = { 0 };
    if (ctx->status != QJS_INTL_OK || choice >= count ||
        first >= ctx->replacements.record_count || count > ctx->replacements.record_count - first) {
        ctx->status = QJS_INTL_DATA_ERROR; return empty;
    }
    return record_text(ctx, &ctx->replacements, first + choice, 0);
}
static int field_set(LocaleContext *ctx, char **field, const char *value)
{
    char *p = value ? ctx_strdup(ctx, value) : NULL;
    if (value && !p) return -1;
    ctx_free(ctx, *field); *field = p; return 0;
}
static int alias_well_formed(const char *s)
{
    char token[9]; const char *p = s; size_t n; int stage = 0;
    while (*p) {
        const char *end = strchr(p, '-');
        n = end ? (size_t)(end - p) : strlen(p);
        if (!n || n > 8) return 0;
        memcpy(token, p, n); token[n] = 0;
        if (!stage) { if (!language_subtag(token)) return 0; stage = 1; }
        else if (stage == 1 && script_subtag(token)) stage = 2;
        else if (stage <= 2 && region_subtag(token)) stage = 3;
        else { if (!variant_subtag(token)) return 0; stage = 4; }
        if (!end) return 1;
        p = end + 1;
    }
    return 0;
}
static int language_matches(const IntlLanguageId *id, const IntlLanguageId *pattern)
{
    size_t i;
    if (strcmp(pattern->language, "und") && strcmp(id->language, pattern->language)) return 0;
    if (pattern->script && (!id->script || strcmp(id->script, pattern->script))) return 0;
    if (pattern->region && (!id->region || strcmp(id->region, pattern->region))) return 0;
    for (i = 0; i < pattern->variants.count; i++)
        if (!list_contains(&id->variants, pattern->variants.items[i])) return 0;
    return 1;
}
static int language_replace(LocaleContext *ctx, IntlLanguageId *id,
                            const IntlLanguageId *pattern, const IntlLanguageId *target)
{
    size_t i, out = 0;
    if (strcmp(target->language, "und") && field_set(ctx, &id->language, target->language) < 0) return -1;
    if ((pattern->script || !id->script) && field_set(ctx, &id->script, target->script) < 0) return -1;
    if ((pattern->region || !id->region) && field_set(ctx, &id->region, target->region) < 0) return -1;
    for (i = 0; i < id->variants.count; i++) {
        if (list_contains(&pattern->variants, id->variants.items[i])) ctx_free(ctx, id->variants.items[i]);
        else id->variants.items[out++] = id->variants.items[i];
    }
    id->variants.count = out;
    for (i = 0; i < target->variants.count; i++)
        if (!list_contains(&id->variants, target->variants.items[i]) &&
            list_append(ctx, &id->variants, target->variants.items[i]) < 0) return -1;
    return intl_list_sort_unique(ctx, &id->variants);
}
/* Lookup preserves the exact schema key casing and uses binary search. */
static int likely_lookup(LocaleContext *ctx, const char *language, const char *script,
                         const char *region, IntlLanguageId *out)
{
    char key[32], sc[5], rg[4]; size_t n, i;
    uint32_t lo = 0, hi = ctx->likely.record_count;
    strcpy(key, language);
    if (script) {
        strcpy(sc, script); sc[0] = (char)(ascii_lower(sc[0]) - 32);
        strcat(key, "-"); strcat(key, sc);
    }
    if (region) {
        strcpy(rg, region);
        for (i = 0; rg[i]; i++) if (ascii_alpha(rg[i])) rg[i] = (char)(ascii_lower(rg[i]) - 32);
        strcat(key, "-"); strcat(key, rg);
    }
    n = strlen(key);
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        QJSIntlDataSlice from = record_text(ctx, &ctx->likely, mid, 0);
        size_t common = n < from.length ? n : from.length;
        int cmp;
        if (ctx->status != QJS_INTL_OK) return -1;
        cmp = memcmp(key, from.data, common);
        if (!cmp) cmp = n < from.length ? -1 : n > from.length;
        if (cmp < 0) hi = mid;
        else if (cmp > 0) lo = mid + 1;
        else {
            IntlTag parsed = { 0 };
            QJSIntlDataSlice target = record_text(ctx, &ctx->likely, mid, 8);
            if (ctx->status != QJS_INTL_OK) return -1;
            if (intl_parse_tag(ctx, (const char *)target.data, target.length, &parsed) < 0) {
                if (ctx->status == QJS_INTL_INVALID_ARGUMENT) ctx->status = QJS_INTL_DATA_ERROR;
                return -1;
            }
            if (!parsed.base.script || !parsed.base.region || parsed.count || parsed.private_use || parsed.base.variants.count) {
                intl_tag_free(ctx, &parsed); ctx->status = QJS_INTL_DATA_ERROR; return -1;
            }
            *out = parsed.base; memset(&parsed.base, 0, sizeof(parsed.base));
            intl_tag_free(ctx, &parsed); return 1;
        }
    }
    return 0;
}
/* Unicode lookup order: L-S-R, L-S, L-R, L, und-S, und-R, und.
 * Unknown language/script/region components survive the matched fallback.
 * Zzzz/ZZ are missing markers only for this operation.
 */
static int maximize_language(LocaleContext *ctx, IntlLanguageId *id)
{
    IntlLanguageId match = { 0 }; int found = 0;
    const char *script = id->script && strcmp(id->script, "zzzz") ? id->script : NULL;
    const char *region = id->region && strcmp(id->region, "zz") ? id->region : NULL;
    if (strcmp(id->language, "und") && script && region) return 0;
    if (script && region) found = likely_lookup(ctx, id->language, script, region, &match);
    if (!found && script) found = likely_lookup(ctx, id->language, script, NULL, &match);
    if (!found && region) found = likely_lookup(ctx, id->language, NULL, region, &match);
    if (!found) found = likely_lookup(ctx, id->language, NULL, NULL, &match);
    if (!found && script) found = likely_lookup(ctx, "und", script, NULL, &match);
    if (!found && region) found = likely_lookup(ctx, "und", NULL, region, &match);
    if (!found) found = likely_lookup(ctx, "und", NULL, NULL, &match);
    if (found < 0) return -1;
    if (!found) { ctx->status = QJS_INTL_DATA_ERROR; return -1; }
    if ((!strcmp(id->language, "und") && field_set(ctx, &id->language, match.language) < 0) ||
        (!script && field_set(ctx, &id->script, match.script) < 0) ||
        (!region && field_set(ctx, &id->region, match.region) < 0)) {
        language_free(ctx, &match); return -1;
    }
    language_free(ctx, &match); return 0;
}
static int canonicalize_language(LocaleContext *ctx, IntlLanguageId *id)
{
    uint32_t pass, i;
    intl_list_sort_unique(ctx, &id->variants);
    for (pass = 0; pass <= ctx->aliases.record_count; pass++) {
        IntlTag best = { 0 }; uint32_t best_index = UINT32_MAX;
        size_t best_specificity = 0;
        char *before = intl_language_string(ctx, id), *after;
        int changed;
        if (!before) return -1;
        for (i = 0; i < ctx->aliases.record_count; i++) {
            IntlTag pattern = { 0 }; char *source; size_t specificity;
            QJSIntlDataSlice from;
            if (record_number(ctx, &ctx->aliases, i, 0) != 0) continue;
            from = record_text(ctx, &ctx->aliases, i, 4);
            if (ctx->status != QJS_INTL_OK) goto fail;
            {
                QJSIntlDataSlice language = from; size_t n = 0;
                while (n < from.length && from.data[n] != '-') n++;
                language.length = n;
                if (!slice_equal(language, "und") && !slice_equal(language, id->language)) continue;
            }
            source = slice_copy(ctx, from);
            if (!source) goto fail;
            /* CLDR contains legacy/extlang rules; they never admit a tag. */
            if (!alias_well_formed(source)) { ctx_free(ctx, source); continue; }
            if (intl_parse_tag(ctx, source, strlen(source), &pattern) < 0) { ctx_free(ctx, source); goto fail; }
            ctx_free(ctx, source);
            specificity = !!strcmp(pattern.base.language, "und") + !!pattern.base.script +
                          !!pattern.base.region + pattern.base.variants.count;
            if (language_matches(id, &pattern.base) &&
                (best_index == UINT32_MAX || specificity > best_specificity)) {
                intl_tag_free(ctx, &best); best = pattern;
                memset(&pattern, 0, sizeof(pattern)); best_index = i; best_specificity = specificity;
            }
            intl_tag_free(ctx, &pattern);
        }
        if (best_index != UINT32_MAX) {
            IntlTag target = { 0 };
            QJSIntlDataSlice text = replacement(ctx, best_index, 0);
            if (ctx->status != QJS_INTL_OK) goto fail;
            if (intl_parse_tag(ctx, (const char *)text.data, text.length, &target) < 0) {
                if (ctx->status == QJS_INTL_INVALID_ARGUMENT) ctx->status = QJS_INTL_DATA_ERROR;
                goto fail;
            }
            if (target.count || target.private_use) {
                intl_tag_free(ctx, &target); ctx->status = QJS_INTL_DATA_ERROR; goto fail;
            }
            if (language_replace(ctx, id, &best.base, &target.base) < 0) {
                intl_tag_free(ctx, &target); goto fail;
            }
            intl_tag_free(ctx, &target);
        } else {
            for (i = 0; i < ctx->aliases.record_count; i++) {
                uint32_t kind = record_number(ctx, &ctx->aliases, i, 0);
                QJSIntlDataSlice from = record_text(ctx, &ctx->aliases, i, 4), to;
                char **field = kind == 1 ? &id->script : kind == 2 ? &id->region : NULL;
                size_t j;
                if (field && *field && slice_equal(from, *field)) {
                    uint32_t choice = 0, count = record_number(ctx, &ctx->aliases, i, 16);
                    if (kind == 2 && count > 1) {
                        IntlLanguageId likely = { 0 };
                        likely.language = ctx_strdup(ctx, id->language);
                        likely.script = id->script ? ctx_strdup(ctx, id->script) : NULL;
                        if (!likely.language || (id->script && !likely.script) || maximize_language(ctx, &likely) < 0) {
                            language_free(ctx, &likely); goto fail;
                        }
                        for (choice = 0; choice < count; choice++)
                            if (slice_equal(replacement(ctx, i, choice), likely.region)) break;
                        if (choice == count) choice = 0;
                        language_free(ctx, &likely);
                    }
                    to = replacement(ctx, i, choice);
                    {
                        char *value = slice_copy(ctx, to);
                        if (!value) goto fail;
                        ctx_free(ctx, *field); *field = value;
                    }
                }
                if (kind == 3) for (j = 0; j < id->variants.count; j++)
                    if (slice_equal(from, id->variants.items[j])) {
                        char *value = slice_copy(ctx, replacement(ctx, i, 0));
                        if (!value) goto fail;
                        ctx_free(ctx, id->variants.items[j]); id->variants.items[j] = value;
                    }
                if (ctx->status != QJS_INTL_OK) goto fail;
            }
        }
        intl_list_sort_unique(ctx, &id->variants);
        after = intl_language_string(ctx, id);
        if (!after) goto fail;
        changed = strcmp(before, after); ctx_free(ctx, before); ctx_free(ctx, after);
        intl_tag_free(ctx, &best);
        if (!changed) return 0;
        continue;
fail:
        ctx_free(ctx, before); intl_tag_free(ctx, &best); return -1;
    }
    ctx->status = QJS_INTL_DATA_ERROR; return -1;
}
static int alias_token_matches(QJSIntlDataSlice aliases, const char *value)
{
    size_t p = 0, start, i, n = strlen(value);
    while (p < aliases.length) {
        while (p < aliases.length && aliases.data[p] == ' ') p++;
        start = p; while (p < aliases.length && aliases.data[p] != ' ') p++;
        if (p - start != n) continue;
        for (i = 0; i < n; i++)
            if (ascii_lower(aliases.data[start + i]) != ascii_lower((unsigned char)value[i])) break;
        if (i == n) return 1;
    }
    return 0;
}
/* Resolves names and every alias token, then follows explicit preferred links.
 * Deprecated without preferred is still canonical; unknown values survive.
 */
static char *bcp_value(LocaleContext *ctx, char extension, const char *key,
                       const char *value, uint32_t *value_type)
{
    uint32_t i, chosen = UINT32_MAX, pass;
    for (i = 0; i < ctx->bcp.record_count; i++) {
        QJSIntlDataSlice ext = record_text(ctx, &ctx->bcp, i, 0);
        QJSIntlDataSlice k = record_text(ctx, &ctx->bcp, i, 8);
        QJSIntlDataSlice name, aliases;
        if (ctx->status != QJS_INTL_OK) return NULL;
        if (ext.length != 1 || ext.data[0] != (unsigned char)extension || !slice_equal(k, key)) continue;
        name = record_text(ctx, &ctx->bcp, i, 16);
        if (!name.length) {
            if (value_type) *value_type = record_number(ctx, &ctx->bcp, i, 40);
            continue;
        }
        aliases = record_text(ctx, &ctx->bcp, i, 24);
        if (slice_equal(name, value) || alias_token_matches(aliases, value)) { chosen = i; break; }
    }
    if (ctx->status != QJS_INTL_OK) return NULL;
    if (chosen == UINT32_MAX) return ctx_strdup(ctx, value);
    for (pass = 0; pass <= ctx->bcp.record_count; pass++) {
        QJSIntlDataSlice preferred = record_text(ctx, &ctx->bcp, chosen, 32);
        if (ctx->status != QJS_INTL_OK) return NULL;
        if (!preferred.length) return slice_copy(ctx, record_text(ctx, &ctx->bcp, chosen, 16));
        for (i = 0; i < ctx->bcp.record_count; i++) {
            QJSIntlDataSlice ext = record_text(ctx, &ctx->bcp, i, 0);
            QJSIntlDataSlice k = record_text(ctx, &ctx->bcp, i, 8);
            QJSIntlDataSlice name = record_text(ctx, &ctx->bcp, i, 16);
            if (ctx->status != QJS_INTL_OK) return NULL;
            if (ext.length == 1 && ext.data[0] == (unsigned char)extension && slice_equal(k, key) &&
                preferred.length == name.length && !memcmp(preferred.data, name.data, name.length)) break;
        }
        if (i == ctx->bcp.record_count) break;
        chosen = i;
    }
    ctx->status = QJS_INTL_DATA_ERROR; return NULL;
}
static char *canonicalize_value(LocaleContext *ctx, char extension,
                                const char *key, const char *value)
{
    uint32_t type = 0, pass;
    char *result = bcp_value(ctx, extension, key, value, &type);
    if (!result) return NULL;
    if (type == 1 && strchr(result, '-')) {
        char *p = result; IntlBuffer b = { 0 };
        while (*p) {
            char *end = strchr(p, '-'), *part;
            if (end) *end = 0;
            part = bcp_value(ctx, extension, key, p, NULL);
            if (!part || buffer_subtag(ctx, &b, part) < 0) {
                ctx_free(ctx, part); ctx_free(ctx, b.data); ctx_free(ctx, result); return NULL;
            }
            ctx_free(ctx, part);
            if (!end) break;
            p = end + 1;
        }
        ctx_free(ctx, result); result = b.data;
    }
    if (extension == 'u' && (!strcmp(key, "rg") || !strcmp(key, "sd"))) {
        for (pass = 0; pass <= ctx->aliases.record_count; pass++) {
            uint32_t i; int changed = 0;
            for (i = 0; i < ctx->aliases.record_count; i++) {
                char *next;
                if (record_number(ctx, &ctx->aliases, i, 0) != 4) continue;
                if (!slice_equal(record_text(ctx, &ctx->aliases, i, 4), result)) continue;
                next = slice_copy(ctx, replacement(ctx, i, 0));
                if (!next) { ctx_free(ctx, result); return NULL; }
                if (region_subtag(next)) {
                    IntlBuffer b = { 0 };
                    if (buffer_add(ctx, &b, next) < 0 || buffer_add(ctx, &b, "zzzz") < 0) {
                        ctx_free(ctx, next); ctx_free(ctx, b.data); ctx_free(ctx, result); return NULL;
                    }
                    ctx_free(ctx, next); next = b.data;
                }
                changed = strcmp(result, next) != 0;
                ctx_free(ctx, result); result = next; break;
            }
            if (ctx->status != QJS_INTL_OK) { ctx_free(ctx, result); return NULL; }
            if (!changed) break;
        }
        if (pass > ctx->aliases.record_count) {
            ctx_free(ctx, result); ctx->status = QJS_INTL_DATA_ERROR; return NULL;
        }
    }
    /* Literal true in a transformed field must remain nonempty. */
    if (extension == 'u' && !strcmp(result, "true")) result[0] = 0;
    return result;
}
static int canonicalize_tag(LocaleContext *ctx, IntlTag *tag)
{
    size_t i, j;
    if (canonicalize_language(ctx, &tag->base) < 0) return -1;
    for (i = 0; i < tag->count; i++) {
        IntlExtension *e = &tag->extensions[i];
        if (e->tlang.language && canonicalize_language(ctx, &e->tlang) < 0) return -1;
        intl_list_sort_unique(ctx, &e->attributes);
        for (j = 0; j < e->count; j++) {
            char *value = canonicalize_value(ctx, e->singleton, e->keywords[j].key, e->keywords[j].value);
            if (!value) return -1;
            ctx_free(ctx, e->keywords[j].value); e->keywords[j].value = value;
        }
        if (e->count > 1) qsort(e->keywords, e->count, sizeof(*e->keywords), keyword_compare);
    }
    if (tag->count > 1) qsort(tag->extensions, tag->count, sizeof(*tag->extensions), extension_compare);
    return 0;
}
static int minimize_language(LocaleContext *ctx, IntlLanguageId *id)
{
    IntlLanguageId maximal = { 0 }; size_t attempt;
    maximal.language = ctx_strdup(ctx, id->language);
    maximal.script = id->script ? ctx_strdup(ctx, id->script) : NULL;
    maximal.region = id->region ? ctx_strdup(ctx, id->region) : NULL;
    if (!maximal.language || (id->script && !maximal.script) || (id->region && !maximal.region) ||
        maximize_language(ctx, &maximal) < 0) goto fail;
    for (attempt = 0; attempt < 3; attempt++) {
        IntlLanguageId trial = { 0 }; int equal;
        trial.language = ctx_strdup(ctx, maximal.language);
        if (attempt == 1) trial.region = ctx_strdup(ctx, maximal.region);
        if (attempt == 2) trial.script = ctx_strdup(ctx, maximal.script);
        if (!trial.language || (attempt == 1 && !trial.region) || (attempt == 2 && !trial.script) ||
            maximize_language(ctx, &trial) < 0) { language_free(ctx, &trial); goto fail; }
        equal = !strcmp(trial.language, maximal.language) && !strcmp(trial.script, maximal.script) &&
                !strcmp(trial.region, maximal.region);
        language_free(ctx, &trial);
        if (equal) {
            if (field_set(ctx, &id->language, maximal.language) < 0 ||
                field_set(ctx, &id->script, attempt == 2 ? maximal.script : NULL) < 0 ||
                field_set(ctx, &id->region, attempt == 1 ? maximal.region : NULL) < 0) goto fail;
            language_free(ctx, &maximal); return 0;
        }
    }
    if (field_set(ctx, &id->language, maximal.language) < 0 ||
        field_set(ctx, &id->script, maximal.script) < 0 ||
        field_set(ctx, &id->region, maximal.region) < 0) goto fail;
    language_free(ctx, &maximal); return 0;
fail:
    language_free(ctx, &maximal); return -1;
}
QJSIntlStatus qjs_intl_native_locale_validate(const QJSIntlAllocator *allocator,
                                             QJSIntlBytes input, int *well_formed)
{
    LocaleContext ctx; IntlTag tag; int parsed;
    if (well_formed) *well_formed = 0;
    if (!well_formed || context_init(&ctx, allocator, NULL, 0) < 0) return QJS_INTL_INVALID_ARGUMENT;
    parsed = intl_parse_tag(&ctx, input.data, input.length, &tag);
    if (!parsed) { *well_formed = 1; intl_tag_free(&ctx, &tag); return QJS_INTL_OK; }
    return ctx.status == QJS_INTL_INVALID_ARGUMENT ? QJS_INTL_OK : ctx.status;
}
static QJSIntlStatus locale_operation(const QJSIntlAllocator *allocator,
                    const QJSIntlDataView *view, QJSIntlBytes input, char **out, int operation)
{
    LocaleContext ctx; IntlTag tag;
    if (out) *out = NULL;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    if (context_init(&ctx, allocator, view, 1) < 0)
        return ctx.status ? ctx.status : QJS_INTL_INVALID_ARGUMENT;
    if (intl_parse_tag(&ctx, input.data, input.length, &tag) < 0) return ctx.status;
    if (canonicalize_tag(&ctx, &tag) < 0) goto done;
    if (operation == 1 && maximize_language(&ctx, &tag.base) < 0) goto done;
    if (operation == 2 && minimize_language(&ctx, &tag.base) < 0) goto done;
    *out = intl_tag_string(&ctx, &tag);
done:
    intl_tag_free(&ctx, &tag); return ctx.status;
}
QJSIntlStatus qjs_intl_native_locale_canonicalize(const QJSIntlAllocator *allocator,
                      const QJSIntlDataView *view, QJSIntlBytes input, char **out)
{
    return locale_operation(allocator, view, input, out, 0);
}
QJSIntlStatus qjs_intl_native_locale_maximize(const QJSIntlAllocator *allocator,
                      const QJSIntlDataView *view, QJSIntlBytes input, char **out)
{
    return locale_operation(allocator, view, input, out, 1);
}
QJSIntlStatus qjs_intl_native_locale_minimize(const QJSIntlAllocator *allocator,
                      const QJSIntlDataView *view, QJSIntlBytes input, char **out)
{
    return locale_operation(allocator, view, input, out, 2);
}
static int unicode_type(const char *s)
{
    size_t count = 0;
    if (!*s) return 1;
    for (;;) {
        if (!*s || *s == '-') {
            if (count < 3 || count > 8) return 0;
            if (!*s) return 1;
            count = 0;
        } else {
            if (!ascii_alnum((unsigned char)*s)) return 0;
            count++;
        }
        s++;
    }
}
QJSIntlStatus qjs_intl_native_locale_canonicalize_uvalue(const QJSIntlAllocator *allocator,
                      const QJSIntlDataView *view, QJSIntlBytes key,
                      QJSIntlBytes value, char **out)
{
    LocaleContext ctx; char k[3], *v;
    if (out) *out = NULL;
    if (!out || !key.data || key.length != 2 || !ascii_alnum((unsigned char)key.data[0]) ||
        !ascii_alpha((unsigned char)key.data[1]) || (!value.data && value.length)) return QJS_INTL_INVALID_ARGUMENT;
    if (context_init(&ctx, allocator, view, 1) < 0)
        return ctx.status ? ctx.status : QJS_INTL_INVALID_ARGUMENT;
    k[0] = ascii_lower((unsigned char)key.data[0]); k[1] = ascii_lower((unsigned char)key.data[1]); k[2] = 0;
    v = copy_n(&ctx, value.data ? value.data : "", value.length);
    if (!v) return ctx.status;
    /* Reject embedded NUL before using string helpers. */
    if (memchr(v, 0, value.length) || !unicode_type(v)) {
        ctx_free(&ctx, v); return QJS_INTL_INVALID_ARGUMENT;
    }
    { size_t i; for (i = 0; i < value.length; i++) v[i] = ascii_lower((unsigned char)v[i]); }
    *out = canonicalize_value(&ctx, 'u', k, v);
    ctx_free(&ctx, v); return ctx.status;
}
