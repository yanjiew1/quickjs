/* Original source-only native DisplayNames engine; no ICU. */
#include "intl/display-names-native.h"
#include "intl/display-names-native-data.h"
#include "intl/native-locale-id.h"
#include <string.h>
#include <limits.h>

struct QJSIntlNativeDisplayNames {
    QJSIntlAllocator allocator;
    QJSIntlDataView view;
    QJSIntlDataSection labels, patterns;
    uint32_t locale;
    QJSIntlDisplayNamesOptions options;
};
typedef struct DNText {
    uint16_t *data;
    size_t length, capacity;
    const QJSIntlAllocator *allocator;
} DNText;

static int alpha(unsigned char c)
{ return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static int digit(unsigned char c) { return c >= '0' && c <= '9'; }
static unsigned char lower(unsigned char c)
{ return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 32) : c; }
static unsigned char upper(unsigned char c)
{ return c >= 'a' && c <= 'z' ? (unsigned char)(c - 32) : c; }
static int all_alpha(const char *s, size_t n)
{ size_t i; for (i = 0; i < n; i++) if (!alpha((unsigned char)s[i])) return 0; return 1; }
static int all_digit(const char *s, size_t n)
{ size_t i; for (i = 0; i < n; i++) if (!digit((unsigned char)s[i])) return 0; return 1; }
static uint32_t u32(const unsigned char *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int cmp(QJSIntlDataSlice a, QJSIntlDataSlice b)
{
    size_t n = a.length < b.length ? a.length : b.length;
    int c = n ? memcmp(a.data, b.data, n) : 0;
    return c ? c : a.length == b.length ? 0 : a.length < b.length ? -1 : 1;
}
static QJSIntlDataSlice bytes(const char *s)
{ QJSIntlDataSlice v; v.data = (const unsigned char *)s; v.length = strlen(s); return v; }
static int ref(QJSIntlNativeDisplayNames *h, const unsigned char *p,
               QJSIntlDataSlice *out)
{ return qjs_intl_data_string(&h->view, u32(p), u32(p + 4), out) == QJS_INTL_DATA_OK; }

/* Strict scalar conversion. Wire text contains no NUL or surrogate scalars;
 * supplementary scalars become a valid UTF16 surrogate pair. */
static int scalar(QJSIntlDataSlice s, size_t *at, uint32_t *value)
{
    uint32_t c, min;
    unsigned n, i;
    if (*at >= s.length) return 0;
    c = s.data[(*at)++];
    if (c < 0x80) { if (!c) return 0; *value = c; return 1; }
    if (c >= 0xc2 && c <= 0xdf) { n = 1; min = 0x80; c &= 31; }
    else if (c >= 0xe0 && c <= 0xef) { n = 2; min = 0x800; c &= 15; }
    else if (c >= 0xf0 && c <= 0xf4) { n = 3; min = 0x10000; c &= 7; }
    else return 0;
    if (s.length - *at < n) return 0;
    for (i = 0; i < n; i++) {
        unsigned char b = s.data[(*at)++];
        if ((b & 0xc0) != 0x80) return 0;
        c = (c << 6) | (b & 63);
    }
    if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return 0;
    *value = c; return 1;
}
static QJSIntlStatus reserve(DNText *t, size_t n)
{
    size_t cap; void *p;
    if (n > SIZE_MAX - t->length) return QJS_INTL_OVERFLOW;
    n += t->length;
    if (n <= t->capacity) return QJS_INTL_OK;
    cap = t->capacity ? t->capacity : 32;
    while (cap < n) { if (cap > SIZE_MAX / 2) { cap = n; break; } cap *= 2; }
    if (cap > SIZE_MAX / sizeof(uint16_t)) return QJS_INTL_OVERFLOW;
    p = t->allocator->realloc(t->allocator->opaque, t->data, cap * sizeof(uint16_t));
    if (!p) return QJS_INTL_NO_MEMORY;
    t->data = p; t->capacity = cap; return QJS_INTL_OK;
}
static QJSIntlStatus append_scalar(DNText *t, uint32_t c)
{
    QJSIntlStatus st = reserve(t, c > 0xffff ? 2 : 1);
    if (st) return st;
    if (c <= 0xffff) t->data[t->length++] = (uint16_t)c;
    else { c -= 0x10000; t->data[t->length++] = (uint16_t)(0xd800 + (c >> 10));
           t->data[t->length++] = (uint16_t)(0xdc00 + (c & 1023)); }
    return QJS_INTL_OK;
}
static QJSIntlStatus append_utf8(DNText *t, QJSIntlDataSlice s)
{
    size_t at = 0; uint32_t c; QJSIntlStatus st;
    while (at < s.length) {
        if (!scalar(s, &at, &c)) return QJS_INTL_DATA_ERROR;
        st = append_scalar(t, c); if (st) return st;
    }
    return QJS_INTL_OK;
}
static QJSIntlStatus append_text(DNText *t, const DNText *s)
{
    QJSIntlStatus st = reserve(t, s->length);
    if (st) return st;
    if (s->length) memcpy(t->data + t->length, s->data, s->length * sizeof(uint16_t));
    t->length += s->length; return QJS_INTL_OK;
}
static void clear_text(DNText *t)
{ if (t->data) t->allocator->free(t->allocator->opaque, t->data); t->data = NULL; t->length = t->capacity = 0; }

/* Pattern literals (including apostrophes, spaces and bidi marks) are exact.
 * CLDR locale display patterns use raw {0}/{1}, not MessageFormat quoting.
 * The opened reader owns global text, pattern and row-order validation. */
static int pattern(QJSIntlNativeDisplayNames *h, uint32_t kind, QJSIntlDataSlice *out)
{
    uint32_t lo = 0, hi = h->patterns.record_count;
    while (lo < hi) {
        uint32_t i = lo + (hi - lo) / 2;
        const unsigned char *p = h->patterns.bytes.data + (size_t)i * 16;
        uint32_t locale = u32(p), k = u32(p + 4);
        if (locale < h->locale || (locale == h->locale && k < kind)) lo = i + 1;
        else hi = i;
    }
    if (lo < h->patterns.record_count) {
        const unsigned char *p = h->patterns.bytes.data + (size_t)lo * 16;
        if (u32(p) == h->locale && u32(p+4) == kind) return ref(h, p+8, out);
    }
    return 0;
}
static int label(QJSIntlNativeDisplayNames *h, uint32_t type, const char *code,
                 QJSIntlDataSlice *out)
{
    uint32_t lo = 0, hi = h->labels.record_count;
    QJSIntlDataSlice key = bytes(code), candidate;
    while (lo < hi) {
        uint32_t i = lo + (hi - lo) / 2;
        const unsigned char *p = h->labels.bytes.data + (size_t)i * 28;
        uint32_t l = u32(p), t = u32(p+4), s = u32(p+8);
        int c;
        if (l != h->locale) c = l < h->locale ? -1 : 1;
        else if (t != type) c = t < type ? -1 : 1;
        else if (s != (uint32_t)h->options.style) c = s < (uint32_t)h->options.style ? -1 : 1;
        else { if (!ref(h, p+12, &candidate)) return 0; c = cmp(candidate, key); }
        if (c < 0) lo = i + 1; else hi = i;
    }
    if (lo < h->labels.record_count) {
        const unsigned char *p = h->labels.bytes.data + (size_t)lo * 28;
        if (u32(p) == h->locale && u32(p+4) == type &&
            u32(p+8) == (uint32_t)h->options.style &&
            ref(h, p+12, &candidate) && !cmp(candidate, key)) return ref(h, p+20, out);
    }
    return 0;
}
/* Rows are sorted by locale/type/style/code. Locate the first row of the
 * requested family without reading strings or scanning unrelated locales.
 * Reader open validated every row before this immutable view was borrowed. */
static int label_group_present(const QJSIntlNativeDisplayNames *h)
{
    uint32_t lo = 0, hi = h->labels.record_count;
    uint32_t type = (uint32_t)h->options.type;
    uint32_t style = (uint32_t)h->options.style;
    while (lo < hi) {
        uint32_t i = lo + (hi - lo) / 2;
        const unsigned char *p = h->labels.bytes.data + (size_t)i * 28;
        uint32_t l = u32(p), t = u32(p + 4), s = u32(p + 8);
        if (l < h->locale || (l == h->locale &&
            (t < type || (t == type && s < style)))) lo = i + 1;
        else hi = i;
    }
    if (lo < h->labels.record_count) {
        const unsigned char *p = h->labels.bytes.data + (size_t)lo * 28;
        return u32(p) == h->locale && u32(p + 4) == type &&
               u32(p + 8) == style;
    }
    return 0;
}
static const char *const fields[] = {
    "era", "year", "quarter", "month", "weekOfYear", "weekday", "day",
    "dayPeriod", "hour", "minute", "second", "timeZoneName"
};
static int valid_language(const char *s)
{
    size_t at = 0, n, start; int script = 0, region = 0, variants = 0;
    while (s[at] && s[at] != '-') at++;
    if (!((at >= 2 && at <= 3) || (at >= 5 && at <= 8)) || !all_alpha(s, at)) return 0;
    while (s[at]) {
        if (s[at++] != '-') return 0;
        start = at; while (s[at] && s[at] != '-') at++; n = at - start;
        if (!variants && !script && !region && n == 4 && all_alpha(s+start, n)) script = 1;
        else if (!variants && !region && ((n == 2 && all_alpha(s+start, n)) ||
                                         (n == 3 && all_digit(s+start, n)))) region = 1;
        else {
            size_t i;
            if (!((n >= 5 && n <= 8) || (n == 4 && digit((unsigned char)s[start])))) return 0;
            for (i = start; i < at; i++) if (!alpha((unsigned char)s[i]) && !digit((unsigned char)s[i])) return 0;
            variants = 1;
        }
    }
    return 1;
}
static QJSIntlStatus canonical(QJSIntlNativeDisplayNames *h, QJSIntlUTF16 input,
                              char **out)
{
    char *s; size_t i, n = input.length;
    QJSIntlStatus st;
    *out = NULL;
    if (!input.data && n) return QJS_INTL_INVALID_ARGUMENT;
    if (n == SIZE_MAX) return QJS_INTL_OVERFLOW;
    s = h->allocator.malloc(h->allocator.opaque, n + 1);
    if (!s) return QJS_INTL_NO_MEMORY;
    for (i = 0; i < n; i++) {
        if (!input.data[i] || input.data[i] > 127) goto invalid;
        s[i] = (char)input.data[i];
    }
    s[n] = 0;
    switch (h->options.type) {
    case QJS_INTL_DN_LANGUAGE: {
        QJSIntlBytes b; int well = 0;
        if (!valid_language(s)) goto invalid;
        b.data = s; b.length = n;
        st = qjs_intl_native_locale_validate(&h->allocator, b, &well);
        if (st || !well) { if (!st) st = QJS_INTL_INVALID_ARGUMENT; goto done; }
        st = qjs_intl_native_locale_canonicalize(&h->allocator, &h->view, b, out);
        goto done;
    }
    case QJS_INTL_DN_REGION:
        if (!((n == 2 && all_alpha(s,n)) || (n == 3 && all_digit(s,n)))) goto invalid;
        for (i = 0; i < n; i++) s[i] = (char)upper((unsigned char)s[i]);
        break;
    case QJS_INTL_DN_SCRIPT:
        if (n != 4 || !all_alpha(s,n)) goto invalid;
        s[0] = (char)upper((unsigned char)s[0]);
        for (i = 1; i < n; i++) s[i] = (char)lower((unsigned char)s[i]);
        break;
    case QJS_INTL_DN_CURRENCY:
        if (n != 3 || !all_alpha(s,n)) goto invalid;
        for (i = 0; i < n; i++) s[i] = (char)upper((unsigned char)s[i]);
        break;
    case QJS_INTL_DN_CALENDAR: {
        size_t start = 0;
        for (i = 0; i <= n; i++) {
            if (i == n || s[i] == '-') {
                if (i-start < 3 || i-start > 8) goto invalid;
                start = i+1;
            } else if (!alpha((unsigned char)s[i]) && !digit((unsigned char)s[i])) goto invalid;
            else s[i] = (char)lower((unsigned char)s[i]);
        }
        break;
    }
    case QJS_INTL_DN_DATE_TIME_FIELD:
        for (i = 0; i < sizeof(fields)/sizeof(fields[0]); i++) if (!strcmp(s, fields[i])) break;
        if (i == sizeof(fields)/sizeof(fields[0])) goto invalid;
        break;
    default: goto invalid;
    }
    *out = s; return QJS_INTL_OK;
invalid:
    st = QJS_INTL_INVALID_ARGUMENT;
done:
    h->allocator.free(h->allocator.opaque, s); return st;
}

static QJSIntlStatus combine(QJSIntlNativeDisplayNames *h, uint32_t kind,
                            const DNText *a, const DNText *b, DNText *out)
{
    QJSIntlDataSlice p, literal;
    size_t i = 0, start = 0; QJSIntlStatus st;
    if (!pattern(h, kind, &p)) return QJS_INTL_DATA_ERROR;
    while (i < p.length) {
        if (p.data[i] == '{') {
            literal.data = p.data + start; literal.length = i - start;
            st = append_utf8(out, literal); if (st) return st;
            st = append_text(out, p.data[i+1] == '0' ? a : b); if (st) return st;
            i += 3; start = i;
        } else i++;
    }
    literal.data = p.data + start; literal.length = i - start;
    return append_utf8(out, literal);
}
static QJSIntlStatus append_name(QJSIntlNativeDisplayNames *h, DNText *t,
                                 QJSIntlDataSlice s)
{
    QJSIntlDataSlice p, replacement; size_t at = 0, pat_at = 0;
    uint32_t c, pc; int ascii = 0, wide = 0; QJSIntlStatus st;
    if (!pattern(h, 0, &p)) return QJS_INTL_DATA_ERROR;
    while (pat_at < p.length) {
        if (!scalar(p, &pat_at, &pc)) return QJS_INTL_DATA_ERROR;
        if (pc == '(') ascii = 1;
        if (pc == 0xff08) wide = 1;
    }
    while (at < s.length) {
        uint32_t kind = UINT32_MAX;
        if (!scalar(s, &at, &c)) return QJS_INTL_DATA_ERROR;
        if (ascii && c == '(') kind = 3;
        else if (ascii && c == ')') kind = 4;
        else if (wide && c == 0xff08) kind = 5;
        else if (wide && c == 0xff09) kind = 6;
        if (kind != UINT32_MAX) {
            if (!pattern(h, kind, &replacement)) return QJS_INTL_DATA_ERROR;
            st = append_utf8(t, replacement);
        } else st = append_scalar(t, c);
        if (st) return st;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus language(QJSIntlNativeDisplayNames *h, const char *code,
                             DNText *out, int *found)
{
    char *copy, *parts[3], *rest, *script = NULL, *region = NULL;
    char dialect[32]; size_t base_len, i, n;
    QJSIntlDataSlice name; DNText base = {0}, extras = {0}, next = {0}, joined = {0};
    QJSIntlStatus st = QJS_INTL_OK;
    base.allocator = extras.allocator = next.allocator = joined.allocator = &h->allocator;
    *found = 0;
    if (h->options.language_dialect && label(h, 0, code, &name)) {
        *found = 1; return append_utf8(out, name);
    }
    n = strlen(code); if (n == SIZE_MAX) return QJS_INTL_OVERFLOW;
    copy = h->allocator.malloc(h->allocator.opaque, n+1);
    if (!copy) return QJS_INTL_NO_MEMORY;
    memcpy(copy, code, n+1);
    rest = strchr(copy, '-'); if (rest) *rest++ = 0;
    base_len = strlen(copy);
    if (rest) {
        char *end = strchr(rest, '-'); size_t length = end ? (size_t)(end-rest) : strlen(rest);
        if (length == 4 && all_alpha(rest, length)) {
            script = rest; if (end) *end++ = 0; rest = end;
        }
    }
    if (rest) {
        char *end = strchr(rest, '-'); size_t length = end ? (size_t)(end-rest) : strlen(rest);
        if ((length == 2 && all_alpha(rest,length)) || (length == 3 && all_digit(rest,length))) {
            region = rest; if (end) *end++ = 0; rest = end;
        }
    }
    /* Dialect candidates prefer language+script+region, language+script,
     * language+region; consumed components do not reappear in parentheses. */
    if (h->options.language_dialect) {
        int attempt;
        for (attempt = 0; attempt < 3; attempt++) {
            char *a = attempt == 2 ? region : script;
            char *b = attempt == 0 ? region : NULL;
            if (!a || (attempt == 0 && !b)) continue;
            n = base_len + 1 + strlen(a) + (b ? 1 + strlen(b) : 0);
            if (n >= sizeof(dialect)) { st = QJS_INTL_DATA_ERROR; goto done; }
            memcpy(dialect,copy,base_len); dialect[base_len] = '-';
            strcpy(dialect+base_len+1,a);
            if (b) { strcat(dialect,"-"); strcat(dialect,b); }
            if (label(h,0,dialect,&name)) {
                if (attempt != 2) script = NULL;
                if (attempt != 1) region = NULL;
                goto base_found;
            }
        }
    }
    if (!label(h,0,copy,&name)) goto done;
base_found:
    /* A simple label is emitted literally. Bracket substitution applies only
     * when the locale pattern actually wraps one or more components. */
    if (!script && !region && !rest) {
        st = append_utf8(out,name); if (!st) *found = 1; goto done;
    }
    st = append_utf8(&base,name); if (st) goto done;
    parts[0] = script; parts[1] = region; parts[2] = rest;
    for (i = 0; i < 3; i++) {
        char *component = parts[i];
        while (component) {
            char *end = i == 2 ? strchr(component,'-') : NULL;
            if (end) *end++ = 0;
            if (!label(h, i == 0 ? 2 : i == 1 ? 1 : 6, component, &name)) goto done;
            st = append_name(h,&next,name); if (st) goto done;
            if (extras.length) {
                st = combine(h,1,&extras,&next,&joined); if (st) goto done;
                clear_text(&extras); extras = joined; memset(&joined,0,sizeof(joined)); joined.allocator = &h->allocator;
                clear_text(&next);
            } else { extras = next; memset(&next,0,sizeof(next)); next.allocator = &h->allocator; }
            component = end;
        }
    }
    if (extras.length) st = combine(h,0,&base,&extras,out);
    else st = append_text(out,&base);
    if (!st) *found = 1;
done:
    clear_text(&base); clear_text(&extras); clear_text(&next); clear_text(&joined);
    h->allocator.free(h->allocator.opaque,copy); return st;
}

QJSIntlStatus qjs_intl_native_display_names_open(const QJSIntlAllocator *a,
    const QJSIntlDataView *v, uint32_t locale,
    const QJSIntlDisplayNamesOptions *o, QJSIntlNativeDisplayNames **out)
{
    QJSIntlNativeDisplayNames h, *allocated;
    QJSIntlDataSection locales; QJSIntlDataStatus ds;
    uint32_t i;
    QJSIntlDataSlice name;
    if (out) *out = NULL;
    if (!out || !a || !a->malloc || !a->realloc || !a->free || !v || !o ||
        (unsigned)o->type > 5 || (unsigned)o->style > 2 ||
        (o->fallback_code != 0 && o->fallback_code != 1) ||
        (o->language_dialect != 0 && o->language_dialect != 1)) return QJS_INTL_INVALID_ARGUMENT;
    memset(&h,0,sizeof(h)); h.allocator = *a; h.view = *v; h.locale = locale; h.options = *o;
    if (qjs_intl_data_section(v,QJS_INTL_DATA_LOCALE,&locales) != QJS_INTL_DATA_OK || locale >= locales.record_count)
        return QJS_INTL_INVALID_ARGUMENT;
    ds = qjs_intl_data_section(v,50,&h.labels);
    if (ds == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (ds != QJS_INTL_DATA_OK || h.labels.record_width != 28) return QJS_INTL_DATA_ERROR;
    ds = qjs_intl_data_section(v,51,&h.patterns);
    if (ds == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (ds != QJS_INTL_DATA_OK || h.patterns.record_width != 16) return QJS_INTL_DATA_ERROR;
    /* Successful qjs_intl_data_open already validated all labels/patterns.
     * Keep capability checks local to this constructor; repeating global
     * validation here would make each open proportional to the whole blob. */
    if (!label_group_present(&h)) return QJS_INTL_UNSUPPORTED;
    if (o->type == QJS_INTL_DN_LANGUAGE) {
        for (i = 0; i < 7; i++) if (!pattern(&h,i,&name)) return QJS_INTL_UNSUPPORTED;
    }
    if (o->type == QJS_INTL_DN_DATE_TIME_FIELD) {
        for (i = 0; i < sizeof(fields)/sizeof(fields[0]); i++)
            if (!label(&h,5,fields[i],&name)) return QJS_INTL_UNSUPPORTED;
    }
    allocated = a->malloc(a->opaque,sizeof(*allocated));
    if (!allocated) return QJS_INTL_NO_MEMORY;
    *allocated = h; *out = allocated; return QJS_INTL_OK;
}
void qjs_intl_native_display_names_close(QJSIntlNativeDisplayNames *h)
{ if (h) h->allocator.free(h->allocator.opaque,h); }
void qjs_intl_native_display_names_result_clear(const QJSIntlAllocator *a,
                                               QJSIntlDisplayNamesResult *r)
{
    if (!r) return;
    if (r->text && a && a->free) a->free(a->opaque,r->text);
    memset(r,0,sizeof(*r));
}
QJSIntlStatus qjs_intl_native_display_names_of(QJSIntlNativeDisplayNames *h,
    QJSIntlUTF16 input, QJSIntlDisplayNamesResult *out)
{
    char *code = NULL; QJSIntlStatus st; QJSIntlDataSlice name;
    DNText text = {0}; int found = 0;
    if (out) memset(out,0,sizeof(*out));
    if (!h || !out) return QJS_INTL_INVALID_ARGUMENT;
    text.allocator = &h->allocator;
    st = canonical(h,input,&code); if (st) return st;
    if (h->options.type == QJS_INTL_DN_LANGUAGE) st = language(h,code,&text,&found);
    else if (label(h,(uint32_t)h->options.type,code,&name)) {
        found = 1; st = append_utf8(&text,name);
    }
    if (!st && !found) {
        clear_text(&text);
        if (h->options.fallback_code) { found = 1; st = append_utf8(&text,bytes(code)); }
    }
    h->allocator.free(h->allocator.opaque,code);
    if (st) { clear_text(&text); return st; }
    if (found) { out->text = text.data; out->length = text.length; out->present = 1; }
    else clear_text(&text);
    return QJS_INTL_OK;
}
