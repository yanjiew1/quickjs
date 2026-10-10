/* ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd, reviewed 2026-10-08.
 * Anchors: sec-iswellformedlanguagetag, sec-canonicalizeunicodelocaleid.
 * Unicode alias data is read through ICU4C's C resource API. */
#include "locale-private.h"
#ifdef CONFIG_INTL
#include <limits.h>
#include <stdlib.h>
#ifndef CONFIG_INTL_NATIVE
#include <unicode/ustring.h>
#endif

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
int js_intl_is_unicode_type(const char *s)
{
#ifdef CONFIG_INTL_NATIVE
    size_t n = 0;
    if (!s || !*s) return 0;
    for (;;) {
        unsigned char c = (unsigned char)*s++;
        if (!c || c == '-') {
            if (n < 3 || n > 8) return 0;
            if (!c) return 1;
            n = 0;
        } else {
            if (!ascii_alnum(c)) return 0;
            n++;
        }
    }
#else
    return s && intl_unicode_type_well_formed(s, strlen(s));
#endif
}
static char *copy_n(JSContext *ctx, const char *s, size_t n)
{
    char *r;
    if (n == SIZE_MAX) { JS_ThrowOutOfMemory(ctx); return NULL; }
    r = js_malloc(ctx, n + 1);
    if (r) { memcpy(r, s, n); r[n] = 0; }
    return r;
}
static int buffer_add(JSContext *ctx, IntlBuffer *b, const char *s)
{
    size_t n = strlen(s), need, cap;
    char *r;
    if (n > SIZE_MAX - b->size - 1) { JS_ThrowOutOfMemory(ctx); return -1; }
    need = b->size + n + 1;
    if (need > b->capacity) {
        cap = b->capacity > SIZE_MAX / 2 ? need : b->capacity * 2;
        if (cap < need) cap = need;
        r = js_realloc(ctx, b->data, cap);
        if (!r) return -1;
        b->data = r; b->capacity = cap;
    }
    memcpy(b->data + b->size, s, n + 1); b->size += n;
    return 0;
}
static int buffer_subtag(JSContext *ctx, IntlBuffer *b, const char *s)
{
    return (b->size && buffer_add(ctx, b, "-") < 0) || buffer_add(ctx, b, s) < 0 ? -1 : 0;
}
static int string_compare(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}
static int list_contains(const JSIntlLocaleList *list, const char *s)
{
    size_t i;
    for (i = 0; i < list->count; i++) if (!strcmp(list->items[i], s)) return 1;
    return 0;
}
int intl_list_sort_unique(JSContext *ctx, JSIntlLocaleList *list)
{
    size_t i, out = 0;
    if (list->count > 1)
        qsort(list->items, list->count, sizeof(*list->items), string_compare);
    for (i = 0; i < list->count; i++) {
        if (out && !strcmp(list->items[out - 1], list->items[i])) js_free(ctx, list->items[i]);
        else list->items[out++] = list->items[i];
    }
    list->count = out;
    return 0;
}
static void language_free(JSContext *ctx, IntlLanguageId *id)
{
    js_free(ctx, id->language); js_free(ctx, id->script); js_free(ctx, id->region);
    js_intl_locale_list_free(ctx, &id->variants); memset(id, 0, sizeof(*id));
}
void intl_tag_free(JSContext *ctx, IntlTag *tag)
{
    size_t i, j;
    language_free(ctx, &tag->base);
    for (i = 0; i < tag->count; i++) {
        IntlExtension *e = &tag->extensions[i];
        language_free(ctx, &e->tlang);
        js_intl_locale_list_free(ctx, &e->attributes);
        for (j = 0; j < e->count; j++) { js_free(ctx, e->keywords[j].key); js_free(ctx, e->keywords[j].value); }
        js_free(ctx, e->keywords); js_free(ctx, e->other);
    }
    js_free(ctx, tag->extensions); js_free(ctx, tag->private_use);
    memset(tag, 0, sizeof(*tag));
}
static int parse_language(JSContext *ctx, char **tokens, size_t end,
                          size_t *position, IntlLanguageId *id)
{
    size_t p = *position;
    if (p == end || !language_subtag(tokens[p])) return 1;
    if (!(id->language = js_intl_strdup(ctx, tokens[p++]))) return -1;
    if (p < end && script_subtag(tokens[p])) {
        if (!(id->script = js_intl_strdup(ctx, tokens[p++]))) return -1;
    }
    if (p < end && region_subtag(tokens[p])) {
        if (!(id->region = js_intl_strdup(ctx, tokens[p++]))) return -1;
    }
    while (p < end && variant_subtag(tokens[p])) {
        if (list_contains(&id->variants, tokens[p])) return 1;
        if (js_intl_locale_list_append(ctx, &id->variants, tokens[p++]) < 0) return -1;
    }
    *position = p;
    return 0;
}
static char *join_tokens(JSContext *ctx, char **tokens, size_t p, size_t end)
{
    IntlBuffer b = { 0 };
    for (; p < end; p++) if (buffer_subtag(ctx, &b, tokens[p]) < 0) { js_free(ctx, b.data); return NULL; }
    if (!b.data) return js_intl_strdup(ctx, "");
    return b.data;
}
static int keyword_set(JSContext *ctx, IntlExtension *e, const char *key,
                       const char *value, BOOL override)
{
    size_t i;
    IntlKeyword *r;
    char *k, *v;
    for (i = 0; i < e->count; i++) if (!strcmp(e->keywords[i].key, key)) {
        if (!override) return 0;
        v = js_intl_strdup(ctx, value); if (!v) return -1;
        js_free(ctx, e->keywords[i].value); e->keywords[i].value = v; return 0;
    }
    if (e->count == SIZE_MAX / sizeof(*r)) { JS_ThrowOutOfMemory(ctx); return -1; }
    k = js_intl_strdup(ctx, key); v = js_intl_strdup(ctx, value);
    if (!k || !v) { js_free(ctx, k); js_free(ctx, v); return -1; }
    r = js_realloc(ctx, e->keywords, (e->count + 1) * sizeof(*r));
    if (!r) { js_free(ctx, k); js_free(ctx, v); return -1; }
    e->keywords = r; r[e->count++] = (IntlKeyword){ k, v }; return 0;
}
static IntlExtension *extension_add(JSContext *ctx, IntlTag *tag, char singleton)
{
    IntlExtension *r;
    if (tag->count == SIZE_MAX / sizeof(*r)) { JS_ThrowOutOfMemory(ctx); return NULL; }
    r = js_realloc(ctx, tag->extensions, (tag->count + 1) * sizeof(*r));
    if (!r) return NULL;
    tag->extensions = r; r += tag->count++; memset(r, 0, sizeof(*r)); r->singleton = singleton; return r;
}
int intl_parse_tag(JSContext *ctx, const char *tag, size_t length, IntlTag *out)
{
    char *storage = NULL, **tokens = NULL;
    size_t count = 1, i, p, end, start;
    unsigned char seen[128] = { 0 };
    int r;
    memset(out, 0, sizeof(*out));
    if (!intl_unicode_locale_well_formed(tag, length)) goto invalid;
    for (i = 0; i < length; i++) {
        if (tag[i] == '-') count++;
        else if (!ascii_alnum((unsigned char)tag[i])) goto invalid;
    }
    if (count > SIZE_MAX / sizeof(*tokens)) goto oom;
    storage = copy_n(ctx, tag, length); tokens = js_malloc(ctx, count * sizeof(*tokens));
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
                if (!list_contains(&e->attributes, tokens[p]) && js_intl_locale_list_append(ctx, &e->attributes, tokens[p]) < 0) goto fail;
                p++;
            }
            while (p < end) {
                const char *key = tokens[p++]; char *value;
                if (strlen(key) != 2 || !ascii_alpha(key[1])) goto invalid;
                start = p; while (p < end && strlen(tokens[p]) >= 3) p++;
                value = join_tokens(ctx, tokens, start, p); if (!value) goto fail;
                r = keyword_set(ctx, e, key, value, FALSE); js_free(ctx, value); if (r < 0) goto fail;
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
                r = keyword_set(ctx, e, key, value, FALSE); js_free(ctx, value); if (r < 0) goto fail;
            }
        } else {
            e->other = join_tokens(ctx, tokens, p, end); if (!e->other) goto fail; p = end;
        }
    }
    js_free(ctx, storage); js_free(ctx, tokens); return 0;
oom:
    JS_ThrowOutOfMemory(ctx); goto fail;
invalid:
    JS_ThrowRangeError(ctx, "invalid Unicode locale identifier");
fail:
    js_free(ctx, storage); js_free(ctx, tokens); intl_tag_free(ctx, out); return -1;
}
static int emit_language(JSContext *ctx, IntlBuffer *b, const IntlLanguageId *id, BOOL lower)
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
char *intl_language_string(JSContext *ctx, const IntlLanguageId *id)
{
    IntlBuffer b = { 0 };
    if (emit_language(ctx, &b, id, FALSE) < 0) { js_free(ctx, b.data); return NULL; }
    return b.data;
}
#ifndef CONFIG_INTL_NATIVE
static int extension_compare(const void *a, const void *b)
{
    return ((const IntlExtension *)a)->singleton - ((const IntlExtension *)b)->singleton;
}
static int keyword_compare(const void *a, const void *b)
{
    return strcmp(((const IntlKeyword *)a)->key, ((const IntlKeyword *)b)->key);
}
#endif
char *intl_tag_string(JSContext *ctx, const IntlTag *tag)
{
    IntlBuffer b = { 0 }; size_t i, j;
    if (emit_language(ctx, &b, &tag->base, FALSE) < 0) goto fail;
    for (i = 0; i < tag->count; i++) {
        const IntlExtension *e = &tag->extensions[i]; char singleton[2] = { e->singleton, 0 };
        if (buffer_subtag(ctx, &b, singleton) < 0) goto fail;
        if (e->tlang.language && emit_language(ctx, &b, &e->tlang, TRUE) < 0) goto fail;
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
    js_free(ctx, b.data); return NULL;
}
const char *intl_tag_keyword(const IntlTag *tag, const char *key)
{
    size_t i, j;
    for (i = 0; i < tag->count; i++) if (tag->extensions[i].singleton == 'u')
        for (j = 0; j < tag->extensions[i].count; j++) if (!strcmp(tag->extensions[i].keywords[j].key, key)) return tag->extensions[i].keywords[j].value;
    return NULL;
}
int intl_tag_set_keyword(JSContext *ctx, IntlTag *tag, const char *key, const char *value)
{
    size_t i;
    IntlExtension *e = NULL;
    for (i = 0; i < tag->count; i++) if (tag->extensions[i].singleton == 'u') e = &tag->extensions[i];
    if (!e && !(e = extension_add(ctx, tag, 'u'))) return -1;
    return keyword_set(ctx, e, key, value, TRUE);
}
#ifdef CONFIG_INTL_NATIVE
char *intl_canonicalize_uvalue(JSContext *ctx, const char *key, const char *value)
{
    QJSIntlProvider *provider = js_intl_native_provider(ctx);
    char *result = NULL;
    QJSIntlStatus status;
    if (!provider) return NULL;
    status = qjs_intl_locale_canonicalize_uvalue(provider,
        (QJSIntlBytes){ key, strlen(key) },
        (QJSIntlBytes){ value, strlen(value) }, &result);
    if (js_intl_native_error(ctx, status, "Unicode locale value")) return NULL;
    return result;
}
char *js_intl_canonicalize_tag(JSContext *ctx, const char *input, size_t length)
{
    QJSIntlProvider *provider = js_intl_native_provider(ctx);
    char *result = NULL;
    QJSIntlStatus status;
    if (!provider) return NULL;
    status = qjs_intl_locale_canonicalize(provider,
        (QJSIntlBytes){ input, length }, &result);
    if (js_intl_native_error(ctx, status, "locale identifier")) return NULL;
    return result;
}
#else
static char *resource_ascii(JSContext *ctx, const UChar *s, int32_t n)
{
    char *r = js_intl_alloc_char(ctx, n); int32_t i;
    if (!r) return NULL;
    for (i = 0; i < n; i++) {
        if (s[i] > 127) { js_free(ctx, r); JS_ThrowInternalError(ctx, "non-ASCII ICU locale metadata"); return NULL; }
        r[i] = ascii_lower(s[i]);
    }
    r[n] = 0; return r;
}
static char *alias_replacement(JSContext *ctx, UResourceBundle *table, const char *key)
{
    UErrorCode status = U_ZERO_ERROR; UResourceBundle *entry; const UChar *value; int32_t n; char *r;
    entry = ures_getByKey(table, key, NULL, &status);
    if (status == U_MISSING_RESOURCE_ERROR) return NULL;
    if (U_FAILURE(status)) { js_intl_icu_error(ctx, status, "locale alias"); return NULL; }
    value = ures_getStringByKey(entry, "replacement", &n, &status);
    if (U_FAILURE(status)) { ures_close(entry); js_intl_icu_error(ctx, status, "locale alias"); return NULL; }
    r = resource_ascii(ctx, value, n); ures_close(entry); return r;
}
/* Alias table keys use underscores and canonical script/region casing. */
static char *alias_tag(JSContext *ctx, const char *key)
{
    char *r = js_intl_strdup(ctx, key), *p;
    if (r) for (p = r; *p; p++) *p = *p == '_' ? '-' : ascii_lower(*p);
    return r;
}
static int language_matches(const IntlLanguageId *id, const IntlLanguageId *pattern)
{
    size_t i;
    if (strcmp(pattern->language, "und") && strcmp(id->language, pattern->language)) return 0;
    if (pattern->script && (!id->script || strcmp(id->script, pattern->script))) return 0;
    if (pattern->region && (!id->region || strcmp(id->region, pattern->region))) return 0;
    for (i = 0; i < pattern->variants.count; i++) if (!list_contains(&id->variants, pattern->variants.items[i])) return 0;
    return 1;
}
static int replace_field(JSContext *ctx, char **field, const char *pattern, const char *replacement)
{
    char *r;
    if (!pattern && *field) return 0;
    r = replacement ? js_intl_strdup(ctx, replacement) : NULL;
    if (replacement && !r) return -1;
    js_free(ctx, *field); *field = r; return 0;
}
static int language_replace(JSContext *ctx, IntlLanguageId *id,
                            const IntlLanguageId *pattern, const IntlLanguageId *replacement)
{
    size_t i, out = 0;
    if (strcmp(replacement->language, "und") && replace_field(ctx, &id->language, pattern->language, replacement->language) < 0) return -1;
    if (replace_field(ctx, &id->script, pattern->script, replacement->script) < 0 ||
        replace_field(ctx, &id->region, pattern->region, replacement->region) < 0) return -1;
    for (i = 0; i < id->variants.count; i++) {
        if (list_contains(&pattern->variants, id->variants.items[i])) js_free(ctx, id->variants.items[i]);
        else id->variants.items[out++] = id->variants.items[i];
    }
    id->variants.count = out;
    for (i = 0; i < replacement->variants.count; i++) if (!list_contains(&id->variants, replacement->variants.items[i]) &&
        js_intl_locale_list_append(ctx, &id->variants, replacement->variants.items[i]) < 0) return -1;
    return intl_list_sort_unique(ctx, &id->variants);
}
/* Skip backwards-compatible LegacyRules. They cannot match this parser's
 * input, and are deliberately not admitted by ECMA402. */
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
static int canonicalize_language(JSContext *ctx, IntlLanguageId *id, UResourceBundle *alias)
{
    UResourceBundle *languages = NULL, *regions = NULL, *scripts = NULL, *variants = NULL;
    UErrorCode status = U_ZERO_ERROR; int result = -1, changed; size_t i;
    languages = ures_getByKey(alias, "language", NULL, &status);
    regions = ures_getByKey(alias, "territory", NULL, &status);
    scripts = ures_getByKey(alias, "script", NULL, &status);
    variants = ures_getByKey(alias, "variant", NULL, &status);
    if (U_FAILURE(status)) { js_intl_icu_error(ctx, status, "locale metadata"); goto done; }
    intl_list_sort_unique(ctx, &id->variants);
    do {
        IntlTag best = { 0 }; char *best_replacement = NULL, *before;
        int best_specificity = -1, k;
        changed = 0;
        before = intl_language_string(ctx, id); if (!before) goto done;
        for (k = 0; k < ures_getSize(languages); k++) {
            UResourceBundle *entry; const char *key; const UChar *u; int32_t n; char *name; IntlTag pattern = { 0 }; int specificity;
            status = U_ZERO_ERROR; entry = ures_getByIndex(languages, k, NULL, &status);
            if (U_FAILURE(status)) { js_intl_icu_error(ctx, status, "locale alias"); js_free(ctx, before); intl_tag_free(ctx, &best); js_free(ctx, best_replacement); goto done; }
            key = ures_getKey(entry);
            {
                const char *separator = strchr(key, '_');
                size_t language_length = separator ? (size_t)(separator - key) : strlen(key);
                if (!((language_length == 3 && !strncmp(key, "und", 3)) ||
                      (strlen(id->language) == language_length && !strncmp(key, id->language, language_length)))) {
                    ures_close(entry); continue;
                }
            }
            name = alias_tag(ctx, key);
            if (!name) { ures_close(entry); js_free(ctx, before); intl_tag_free(ctx, &best); js_free(ctx, best_replacement); goto done; }
            if (!alias_well_formed(name)) { js_free(ctx, name); ures_close(entry); continue; }
            if (intl_parse_tag(ctx, name, strlen(name), &pattern) < 0) { js_free(ctx, name); ures_close(entry); js_free(ctx, before); intl_tag_free(ctx, &best); js_free(ctx, best_replacement); goto done; }
            js_free(ctx, name);
            specificity = strcmp(pattern.base.language, "und") != 0;
            specificity += !!pattern.base.script + !!pattern.base.region + pattern.base.variants.count;
            if (!pattern.count && !pattern.private_use && language_matches(id, &pattern.base) && specificity > best_specificity) {
                char *replacement;
                u = ures_getStringByKey(entry, "replacement", &n, &status);
                if (U_FAILURE(status)) { js_intl_icu_error(ctx, status, "locale alias"); intl_tag_free(ctx, &pattern); ures_close(entry); js_free(ctx, before); intl_tag_free(ctx, &best); js_free(ctx, best_replacement); goto done; }
                replacement = resource_ascii(ctx, u, n);
                if (!replacement) { intl_tag_free(ctx, &pattern); ures_close(entry); js_free(ctx, before); intl_tag_free(ctx, &best); js_free(ctx, best_replacement); goto done; }
                intl_tag_free(ctx, &best); js_free(ctx, best_replacement);
                best = pattern; memset(&pattern, 0, sizeof(pattern)); best_replacement = replacement; best_specificity = specificity;
            }
            intl_tag_free(ctx, &pattern); ures_close(entry);
        }
        if (best_replacement) {
            IntlTag replacement = { 0 }; char *name = alias_tag(ctx, best_replacement);
            if (!name || intl_parse_tag(ctx, name, strlen(name), &replacement) < 0 ||
                language_replace(ctx, id, &best.base, &replacement.base) < 0) {
                js_free(ctx, name); js_free(ctx, before); intl_tag_free(ctx, &replacement); intl_tag_free(ctx, &best); js_free(ctx, best_replacement); goto done;
            }
            js_free(ctx, name); intl_tag_free(ctx, &replacement);
        } else if (id->region) {
            char key[4], *replacement; size_t j;
            strcpy(key, id->region); for (j = 0; key[j]; j++) if (ascii_alpha(key[j])) key[j] -= 32;
            replacement = alias_replacement(ctx, regions, key);
            if (!replacement && JS_HasException(ctx)) { js_free(ctx, before); intl_tag_free(ctx, &best); goto done; }
            if (replacement) {
                char *space = strchr(replacement, ' '), *selected = replacement;
                if (space) {
                    char locale[32], maximal[ULOC_FULLNAME_CAPACITY], likely[4]; int32_t len;
                    snprintf(locale, sizeof(locale), "%s%s%s", id->language, id->script ? "_" : "", id->script ? id->script : "");
                    status = U_ZERO_ERROR; len = uloc_addLikelySubtags(locale, maximal, sizeof(maximal), &status);
                    if (U_FAILURE(status) || len >= (int32_t)sizeof(maximal)) { js_intl_icu_error(ctx, status, "likely subtags"); js_free(ctx, replacement); js_free(ctx, before); intl_tag_free(ctx, &best); js_free(ctx, best_replacement); goto done; }
                    status = U_ZERO_ERROR; uloc_getCountry(maximal, likely, sizeof(likely), &status);
                    for (j = 0; likely[j]; j++) likely[j] = ascii_lower(likely[j]);
                    for (selected = replacement; *selected;) {
                        char *next = strchr(selected, ' '); size_t size = next ? (size_t)(next - selected) : strlen(selected);
                        if (strlen(likely) == size && !strncmp(selected, likely, size)) break;
                        if (!next) { selected = replacement; break; } selected = next + 1;
                    }
                    space = strchr(selected, ' '); if (space) *space = 0;
                }
                if (replace_field(ctx, &id->region, "region", selected) < 0) { js_free(ctx, replacement); js_free(ctx, before); intl_tag_free(ctx, &best); js_free(ctx, best_replacement); goto done; }
                js_free(ctx, replacement);
            }
        }
        if (!best_replacement && id->script) {
            char key[5], *replacement; strcpy(key, id->script); key[0] -= 32;
            replacement = alias_replacement(ctx, scripts, key);
            if (!replacement && JS_HasException(ctx)) { js_free(ctx, before); intl_tag_free(ctx, &best); goto done; }
            if (replacement && replace_field(ctx, &id->script, "script", replacement) < 0) { js_free(ctx, replacement); js_free(ctx, before); intl_tag_free(ctx, &best); goto done; }
            js_free(ctx, replacement);
        }
        if (!best_replacement) for (i = 0; i < id->variants.count; i++) {
            char *replacement = alias_replacement(ctx, variants, id->variants.items[i]);
            if (!replacement && JS_HasException(ctx)) { js_free(ctx, before); intl_tag_free(ctx, &best); goto done; }
            if (replacement) { js_free(ctx, id->variants.items[i]); id->variants.items[i] = replacement; }
        }
        {
            char *after = intl_language_string(ctx, id);
            if (!after) { js_free(ctx, before); intl_tag_free(ctx, &best); js_free(ctx, best_replacement); goto done; }
            changed = strcmp(before, after) != 0; js_free(ctx, before); js_free(ctx, after);
        }
        intl_tag_free(ctx, &best); js_free(ctx, best_replacement);
        intl_list_sort_unique(ctx, &id->variants);
    } while (changed);
    result = 0;
done:
    ures_close(languages); ures_close(regions); ures_close(scripts); ures_close(variants); return result;
}
char *intl_canonicalize_uvalue(JSContext *ctx, const char *key, const char *value)
{
    char *lower = js_intl_strdup(ctx, value), *r; const char *canonical; size_t i;
    if (!lower) return NULL;
    for (i = 0; lower[i]; i++) lower[i] = ascii_lower(lower[i]);
    canonical = !strcmp(key, "ca") ?
        intl_calendar_type_name(lower, strlen(lower)) : NULL;
    if (!canonical)
        canonical = uloc_toUnicodeLocaleType(key, lower);
    if (canonical) { r = js_intl_strdup(ctx, canonical); js_free(ctx, lower); lower = r; if (!lower) return NULL; }
    if (!strcmp(lower, "true")) lower[0] = 0;
    return lower;
}
char *js_intl_canonicalize_tag(JSContext *ctx, const char *input, size_t length)
{
    IntlTag tag; UResourceBundle *metadata = NULL, *alias = NULL, *subdivisions = NULL;
    UErrorCode status = U_ZERO_ERROR; char *result = NULL; size_t i, j;
    if (intl_parse_tag(ctx, input, length, &tag) < 0) return NULL;
    metadata = ures_openDirect(NULL, "metadata", &status); alias = ures_getByKey(metadata, "alias", NULL, &status);
    subdivisions = ures_getByKey(alias, "subdivision", NULL, &status);
    if (U_FAILURE(status)) { js_intl_icu_error(ctx, status, "canonical locale data"); goto done; }
    if (canonicalize_language(ctx, &tag.base, alias) < 0) goto done;
    for (i = 0; i < tag.count; i++) {
        IntlExtension *e = &tag.extensions[i];
        if (e->tlang.language && canonicalize_language(ctx, &e->tlang, alias) < 0) goto done;
        intl_list_sort_unique(ctx, &e->attributes);
        for (j = 0; j < e->count; j++) {
            char *value;
            /* T fields require a nonempty value: preserve literal true. */
            if (e->singleton == 't') {
                const char *canonical = uloc_toUnicodeLocaleType(e->keywords[j].key, e->keywords[j].value);
                value = js_intl_strdup(ctx, canonical ? canonical : e->keywords[j].value);
            } else value = intl_canonicalize_uvalue(ctx, e->keywords[j].key, e->keywords[j].value);
            if (!value) goto done;
            if (!strcmp(e->keywords[j].key, "rg") || !strcmp(e->keywords[j].key, "sd")) {
                char *replacement = alias_replacement(ctx, subdivisions, value);
                if (!replacement && JS_HasException(ctx)) { js_free(ctx, value); goto done; }
                if (replacement) {
                    char *space = strchr(replacement, ' '); if (space) *space = 0;
                    js_free(ctx, value); value = replacement;
                    if (strlen(value) == 2) {
                        IntlBuffer b = { 0 };
                        if (buffer_add(ctx, &b, value) < 0 || buffer_add(ctx, &b, "zzzz") < 0) { js_free(ctx, value); js_free(ctx, b.data); goto done; }
                        js_free(ctx, value); value = b.data;
                    }
                }
            }
            js_free(ctx, e->keywords[j].value); e->keywords[j].value = value;
        }
        if (e->count > 1)
            qsort(e->keywords, e->count, sizeof(*e->keywords), keyword_compare);
    }
    if (tag.count > 1)
        qsort(tag.extensions, tag.count, sizeof(*tag.extensions), extension_compare);
    result = intl_tag_string(ctx, &tag);
done:
    ures_close(subdivisions); ures_close(alias); ures_close(metadata); intl_tag_free(ctx, &tag); return result;
}
#endif /* canonicalization provider */
JSValue intl_array_from_list(JSContext *ctx, const JSIntlLocaleList *list)
{
    JSValue array = JS_NewArray(ctx); size_t i;
    if (JS_IsException(array)) return array;
    for (i = 0; i < list->count; i++) {
        JSValue value = JS_NewString(ctx, list->items[i]);
        if (JS_IsException(value) || JS_DefinePropertyValueInt64(ctx, array, i, value, JS_PROP_C_W_E) < 0) { JS_FreeValue(ctx, array); return JS_EXCEPTION; }
    }
    return array;
}
#endif
