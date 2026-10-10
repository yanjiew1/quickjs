/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Original implementation of Unicode18 UAX29 revision49 default rules.
 * Property data is external; no ICU, JS, normalization or locale inference.
 * Unicode normative text/data attribution is in NOTICE.txt. */
#include "segmenter-native.h"
#include "segmenter-schema.h"
#include <string.h>

typedef struct SegmentPoint {
    size_t offset, previous, next, ri_count;
    QJSIntlSegmentProperties property;
    uint8_t effective, significant, linker, zwj_ep;
    uint8_t terminal_close, terminal_space, lower_ahead;
} SegmentPoint;
typedef struct SegmentBoundary {
    size_t offset;
    uint8_t word_like;
} SegmentBoundary;
struct QJSIntlNativeSegmenter {
    QJSIntlAllocator allocator;
    QJSIntlSegmentPropertySource properties;
    QJSIntlSegmentGranularity granularity;
};
struct QJSIntlNativeSegments {
    QJSIntlAllocator allocator;
    uint16_t *text;
    size_t length, count;
    SegmentBoundary *boundary; /* count+1 entries, final offset=length */
    QJSIntlSegmentGranularity granularity;
};
static int allocator_valid(const QJSIntlAllocator *a)
{
    return a && a->malloc && a->realloc && a->free;
}
static int multiply_ok(size_t n, size_t width)
{
    return n <= SIZE_MAX / width;
}
static int word_ignore(unsigned p)
{
    return p == QJS_WB_EXTEND || p == QJS_WB_FORMAT || p == QJS_WB_ZWJ;
}
static int word_newline(unsigned p)
{
    return p == QJS_WB_CR || p == QJS_WB_LF || p == QJS_WB_NEWLINE;
}
static int sentence_ignore(unsigned p)
{
    return p == QJS_SB_EXTEND || p == QJS_SB_FORMAT;
}
static int sentence_paragraph(unsigned p)
{
    return p == QJS_SB_CR || p == QJS_SB_LF || p == QJS_SB_SEP;
}
static int sentence_terminal(unsigned p)
{
    return p == QJS_SB_ATERM || p == QJS_SB_STERM;
}
static int ahletter(unsigned p)
{
    return p == QJS_WB_ALETTER || p == QJS_WB_HEBREW_LETTER;
}
static int letter_mid(unsigned p)
{
    return p == QJS_WB_MIDLETTER || p == QJS_WB_MIDNUMLET ||
           p == QJS_WB_SINGLE_QUOTE;
}
static int numeric_mid(unsigned p)
{
    return p == QJS_WB_MIDNUM || p == QJS_WB_MIDNUMLET ||
           p == QJS_WB_SINGLE_QUOTE;
}
static int extendnum_base(unsigned p)
{
    return ahletter(p) || p == QJS_WB_NUMERIC || p == QJS_WB_KATAKANA;
}
static int grapheme_control(unsigned p)
{
    return p == QJS_GCB_CR || p == QJS_GCB_LF || p == QJS_GCB_CONTROL;
}
static int grapheme_break(const SegmentPoint *c, size_t i)
{
    unsigned a = c[i-1].property.grapheme, b = c[i].property.grapheme;
    if (a == QJS_GCB_CR && b == QJS_GCB_LF) return 0; /* GB3 */
    if (grapheme_control(a) || grapheme_control(b)) return 1; /* GB4/5 */
    if (a == QJS_GCB_L && (b == QJS_GCB_L || b == QJS_GCB_V ||
                          b == QJS_GCB_LV || b == QJS_GCB_LVT)) return 0; /* GB6 */
    if ((a == QJS_GCB_LV || a == QJS_GCB_V) &&
        (b == QJS_GCB_V || b == QJS_GCB_T)) return 0; /* GB7 */
    if ((a == QJS_GCB_LVT || a == QJS_GCB_T) && b == QJS_GCB_T) return 0; /* GB8 */
    if (b == QJS_GCB_EXTEND || b == QJS_GCB_ZWJ) return 0; /* GB9 */
    if (b == QJS_GCB_SPACING_MARK || a == QJS_GCB_PREPEND) return 0; /* GB9a/b */
    /* Unicode18 GB9c: Linker Extend* x Consonant, changed in revision49. */
    if (c[i-1].linker && c[i].property.indic_conjunct == QJS_INCB_CONSONANT)
        return 0;
    if (a == QJS_GCB_ZWJ && c[i-1].zwj_ep &&
        c[i].property.extended_pictographic) return 0; /* GB11 */
    if (a == QJS_GCB_RI && b == QJS_GCB_RI &&
        (c[i-1].ri_count & 1)) return 0; /* GB12/13 */
    return 1; /* GB999 */
}
static int word_break(const SegmentPoint *c, size_t i)
{
    unsigned raw_a = c[i-1].property.word, b = c[i].property.word, a, p = 0, n = 0;
    size_t left = c[i].previous, before, after;
    if (raw_a == QJS_WB_CR && b == QJS_WB_LF) return 0; /* WB3 */
    if (word_newline(raw_a) || word_newline(b)) return 1; /* WB3a/b */
    if (raw_a == QJS_WB_ZWJ && c[i].property.extended_pictographic) return 0; /* WB3c */
    if (raw_a == QJS_WB_WSEGSPACE && b == QJS_WB_WSEGSPACE) return 0; /* WB3d */
    if (word_ignore(b)) return 0; /* WB4, earlier rules retain precedence */
    if (left == SIZE_MAX) return 1;
    a = c[left].effective;
    before = c[left].previous;
    after = c[i].next;
    if (before != SIZE_MAX) p = c[before].effective;
    if (after != SIZE_MAX) n = c[after].effective;
    if (ahletter(a) && ahletter(b)) return 0; /* WB5 */
    if (ahletter(a) && letter_mid(b) && ahletter(n)) return 0; /* WB6 */
    if (ahletter(p) && letter_mid(a) && ahletter(b)) return 0; /* WB7 */
    if (a == QJS_WB_HEBREW_LETTER && b == QJS_WB_SINGLE_QUOTE) return 0; /* WB7a */
    if (a == QJS_WB_HEBREW_LETTER && b == QJS_WB_DOUBLE_QUOTE &&
        n == QJS_WB_HEBREW_LETTER) return 0; /* WB7b */
    if (p == QJS_WB_HEBREW_LETTER && a == QJS_WB_DOUBLE_QUOTE &&
        b == QJS_WB_HEBREW_LETTER) return 0; /* WB7c */
    if (a == QJS_WB_NUMERIC && b == QJS_WB_NUMERIC) return 0; /* WB8 */
    if ((ahletter(a) && b == QJS_WB_NUMERIC) ||
        (a == QJS_WB_NUMERIC && ahletter(b))) return 0; /* WB9/10 */
    if (p == QJS_WB_NUMERIC && numeric_mid(a) && b == QJS_WB_NUMERIC) return 0; /* WB11 */
    if (a == QJS_WB_NUMERIC && numeric_mid(b) && n == QJS_WB_NUMERIC) return 0; /* WB12 */
    if (a == QJS_WB_KATAKANA && b == QJS_WB_KATAKANA) return 0; /* WB13 */
    if ((extendnum_base(a) || a == QJS_WB_EXTENDNUMLET) &&
        b == QJS_WB_EXTENDNUMLET) return 0; /* WB13a */
    if (a == QJS_WB_EXTENDNUMLET && extendnum_base(b)) return 0; /* WB13b */
    if (a == QJS_WB_RI && b == QJS_WB_RI && (c[left].ri_count & 1)) return 0; /* WB15/16 */
    return 1; /* WB999 */
}
static int sentence_break(const SegmentPoint *c, size_t i)
{
    unsigned raw_a = c[i-1].property.sentence, b = c[i].property.sentence;
    unsigned a = QJS_SB_OTHER, p = QJS_SB_OTHER;
    unsigned tc = c[i-1].terminal_close, ts = c[i-1].terminal_space;
    size_t left = c[i].previous, before;
    if (raw_a == QJS_SB_CR && b == QJS_SB_LF) return 0; /* SB3 */
    if (sentence_paragraph(raw_a)) return 1; /* SB4 */
    if (sentence_ignore(b)) return 0; /* SB5 */
    if (left != SIZE_MAX) {
        a = c[left].effective;
        before = c[left].previous;
        if (before != SIZE_MAX) p = c[before].effective;
    }
    if (a == QJS_SB_ATERM && b == QJS_SB_NUMERIC) return 0; /* SB6 */
    if ((p == QJS_SB_UPPER || p == QJS_SB_LOWER) &&
        a == QJS_SB_ATERM && b == QJS_SB_UPPER) return 0; /* SB7 */
    if (ts == QJS_SB_ATERM && c[i].lower_ahead) return 0; /* SB8 */
    if (ts && (b == QJS_SB_SCONTINUE || sentence_terminal(b))) return 0; /* SB8a */
    if (tc && (b == QJS_SB_CLOSE || b == QJS_SB_SP || sentence_paragraph(b))) return 0; /* SB9 */
    if (ts && (b == QJS_SB_SP || sentence_paragraph(b))) return 0; /* SB10 */
    if (ts) return 1; /* SB11, ParaSep is already handled by SB4 */
    return 0; /* SB998 */
}
static void prepare_context(SegmentPoint *c, size_t count, QJSIntlSegmentGranularity g)
{
    size_t i, previous = SIZE_MAX, next = SIZE_MAX, ri = 0;
    unsigned linker = 0, ep = 0, tc = 0, ts = 0, lower = 0;
    for (i = 0; i < count; i++) {
        QJSIntlSegmentProperties p = c[i].property;
        unsigned value, ignored, paragraph_before;
        if (g == QJS_INTL_SEGMENT_GRAPHEME) {
            c[i].zwj_ep = p.grapheme == QJS_GCB_ZWJ && ep;
            if (p.extended_pictographic) ep = 1;
            else if (p.grapheme != QJS_GCB_EXTEND) ep = 0;
            if (p.indic_conjunct != QJS_INCB_EXTEND)
                linker = p.indic_conjunct == QJS_INCB_LINKER;
            c[i].linker = (uint8_t)linker;
            ri = p.grapheme == QJS_GCB_RI ? ri + 1 : 0;
            c[i].ri_count = ri;
            continue;
        }
        value = g == QJS_INTL_SEGMENT_WORD ? p.word : p.sentence;
        ignored = g == QJS_INTL_SEGMENT_WORD ? word_ignore(value) : sentence_ignore(value);
        paragraph_before = i == 0 || (g == QJS_INTL_SEGMENT_WORD ?
            word_newline(c[i-1].property.word) : sentence_paragraph(c[i-1].property.sentence));
        /* Leading ignored runs and runs immediately after paragraph separators
         * begin their own Other anchor (UAX29 section6.2). */
        c[i].significant = !ignored || paragraph_before;
        c[i].effective = (uint8_t)(ignored ? 0 : value);
        c[i].previous = previous;
        if (c[i].significant) {
            previous = i;
            if (g == QJS_INTL_SEGMENT_WORD) {
                ri = value == QJS_WB_RI ? ri + 1 : 0;
            } else {
                value = c[i].effective;
                if (sentence_terminal(value)) tc = ts = value;
                else if (value == QJS_SB_CLOSE) ts = tc;
                else if (value == QJS_SB_SP) tc = 0;
                else tc = ts = 0;
            }
        }
        c[i].ri_count = ri;
        c[i].terminal_close = (uint8_t)tc;
        c[i].terminal_space = (uint8_t)ts;
    }
    for (i = count; i > 0;) {
        unsigned v;
        --i;
        c[i].next = next;
        if (c[i].significant) next = i;
        if (g != QJS_INTL_SEGMENT_SENTENCE) continue;
        v = c[i].effective;
        if (c[i].significant) {
            if (v == QJS_SB_LOWER) lower = 1;
            else if (v == QJS_SB_UPPER || v == QJS_SB_OLETTER ||
                     sentence_paragraph(v) || sentence_terminal(v)) lower = 0;
        }
        c[i].lower_ahead = (uint8_t)lower;
    }
}
QJSIntlStatus qjs_intl_native_segmenter_open_default(const QJSIntlAllocator *a,
    const QJSIntlSegmentPropertySource *p, QJSIntlSegmentGranularity granularity,
    QJSIntlNativeSegmenter **out)
{
    QJSIntlNativeSegmenter *s;
    if (out) *out = NULL;
    if (!out || !allocator_valid(a) || !p || !p->lookup ||
        granularity < QJS_INTL_SEGMENT_GRAPHEME || granularity > QJS_INTL_SEGMENT_SENTENCE)
        return QJS_INTL_INVALID_ARGUMENT;
    if (p->unicode_version != QJS_INTL_SEGMENT_UNICODE_VERSION) return QJS_INTL_UNSUPPORTED;
    s = a->malloc(a->opaque, sizeof(*s));
    if (!s) return QJS_INTL_NO_MEMORY;
    s->allocator = *a; s->properties = *p; s->granularity = granularity;
    *out = s;
    return QJS_INTL_OK;
}
void qjs_intl_native_segmenter_close(QJSIntlNativeSegmenter *s)
{
    if (s) s->allocator.free(s->allocator.opaque, s);
}
void qjs_intl_native_segments_free(QJSIntlNativeSegments *s)
{
    if (!s) return;
    s->allocator.free(s->allocator.opaque, s->text);
    s->allocator.free(s->allocator.opaque, s->boundary);
    s->allocator.free(s->allocator.opaque, s);
}
QJSIntlStatus qjs_intl_native_segments_new(QJSIntlNativeSegmenter *service,
    QJSIntlUTF16 input, QJSIntlNativeSegments **out)
{
    QJSIntlNativeSegments *s;
    SegmentPoint *c = NULL;
    size_t at = 0, count = 0, i, segment = 0;
    QJSIntlStatus status = QJS_INTL_NO_MEMORY;
    QJSIntlAllocator a;
    if (out) *out = NULL;
    if (!out || !service || (!input.data && input.length)) return QJS_INTL_INVALID_ARGUMENT;
    if (input.length == SIZE_MAX || !multiply_ok(input.length, sizeof(*c)) ||
        !multiply_ok(input.length, sizeof(uint16_t)) ||
        !multiply_ok(input.length + 1, sizeof(SegmentBoundary))) return QJS_INTL_OVERFLOW;
    a = service->allocator;
    s = a.malloc(a.opaque, sizeof(*s));
    if (!s) return QJS_INTL_NO_MEMORY;
    memset(s, 0, sizeof(*s));
    s->allocator = a; s->length = input.length; s->granularity = service->granularity;
    s->boundary = a.malloc(a.opaque, (input.length + 1) * sizeof(*s->boundary));
    if (!s->boundary) goto fail;
    memset(s->boundary, 0, (input.length + 1) * sizeof(*s->boundary));
    if (input.length) {
        s->text = a.malloc(a.opaque, input.length * sizeof(*s->text));
        c = a.malloc(a.opaque, input.length * sizeof(*c));
        if (!s->text || !c) goto fail;
        memcpy(s->text, input.data, input.length * sizeof(*s->text));
        memset(c, 0, input.length * sizeof(*c));
    }
    while (at < input.length) {
        uint32_t cp = s->text[at];
        c[count].offset = at++;
        if (cp >= 0xd800 && cp <= 0xdbff && at < input.length &&
            s->text[at] >= 0xdc00 && s->text[at] <= 0xdfff) {
            cp = 0x10000 + ((cp - 0xd800) << 10) + (s->text[at++] - 0xdc00);
        }
        if (!(cp >= 0xd800 && cp <= 0xdfff)) {
            status = service->properties.lookup(service->properties.opaque, cp, &c[count].property);
            if (status != QJS_INTL_OK) goto fail;
        }
        if (c[count].property.grapheme >= QJS_GCB_COUNT ||
            c[count].property.word >= QJS_WB_COUNT || c[count].property.sentence >= QJS_SB_COUNT ||
            c[count].property.indic_conjunct >= QJS_INCB_COUNT ||
            c[count].property.extended_pictographic > 1) {
            status = QJS_INTL_DATA_ERROR; goto fail;
        }
        count++;
    }
    prepare_context(c, count, s->granularity);
    for (i = 0; i < count; i++) {
        int boundary = i == 0 || (s->granularity == QJS_INTL_SEGMENT_GRAPHEME ?
            grapheme_break(c, i) : s->granularity == QJS_INTL_SEGMENT_WORD ?
            word_break(c, i) : sentence_break(c, i));
        if (boundary) {
            segment = s->count++;
            s->boundary[segment].offset = c[i].offset;
        }
        if (s->granularity == QJS_INTL_SEGMENT_WORD && extendnum_base(c[i].property.word))
            s->boundary[segment].word_like = 1;
    }
    s->boundary[s->count].offset = input.length;
    a.free(a.opaque, c);
    *out = s;
    return QJS_INTL_OK;
fail:
    a.free(a.opaque, c);
    qjs_intl_native_segments_free(s);
    return status;
}
QJSIntlUTF16 qjs_intl_native_segments_input(const QJSIntlNativeSegments *s)
{
    QJSIntlUTF16 result = { NULL, 0 };
    if (s) { result.data = s->text; result.length = s->length; }
    return result;
}
static void segment_result(const QJSIntlNativeSegments *s, size_t i, QJSIntlSegmentResult *out)
{
    out->start = s->boundary[i].offset;
    out->end = s->boundary[i+1].offset;
    out->segment.data = s->text + out->start;
    out->segment.length = out->end - out->start;
    out->has_word_like = s->granularity == QJS_INTL_SEGMENT_WORD;
    out->is_word_like = s->boundary[i].word_like;
}
QJSIntlStatus qjs_intl_native_segments_next(const QJSIntlNativeSegments *s,
    size_t *cursor, QJSIntlSegmentResult *out, int *done)
{
    if (out) memset(out, 0, sizeof(*out));
    if (done) *done = 1;
    if (!s || !cursor || !out || !done) return QJS_INTL_INVALID_ARGUMENT;
    if (*cursor >= s->count) return QJS_INTL_OK;
    segment_result(s, *cursor, out);
    ++*cursor; *done = 0;
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_segments_containing(const QJSIntlNativeSegments *s,
    size_t index, QJSIntlSegmentResult *out, int *found)
{
    size_t low = 0, high, mid;
    if (out) memset(out, 0, sizeof(*out));
    if (found) *found = 0;
    if (!s || !out || !found) return QJS_INTL_INVALID_ARGUMENT;
    if (index >= s->length) return QJS_INTL_OK;
    high = s->count;
    while (low + 1 < high) {
        mid = low + (high - low) / 2;
        if (s->boundary[mid].offset <= index) low = mid;
        else high = mid;
    }
    segment_result(s, low, out); *found = 1;
    return QJS_INTL_OK;
}
