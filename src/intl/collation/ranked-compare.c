/* Retain weak levels only while the complete primary streams remain equal.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "ranked-compare.h"
#include "ranked-elements.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include <string.h>

typedef struct Primary {
    uint32_t band, group, rank;
    QJSIntlCollationNumber number;
} Primary;

typedef struct SavedElement {
    QJSIntlCollationRankedElement ce;
    int shifted;
} SavedElement;

typedef struct Stream {
    QJSIntlCollationRankedElements iterator;
    SavedElement *elements;
    size_t count, capacity;
    uint32_t numeric, common_s, common_t, separator, variable_limit;
    int retain, retain_quaternary, shifted, variable_run, any_quaternary;
    SavedElement small[8];
} Stream;

static int compare_u32(uint32_t a, uint32_t b)
{
    return a < b ? -1 : a != b;
}

/* Group membership uses immutable source provenance. Full final ranks remain
   untouched: neither guessed strides nor rank doubling can preserve uint32. */
static Primary primary(const Stream *s, const QJSIntlCollationRankedElement *ce)
{
    const QJSIntlCompiledCollationProgram *p = &s->iterator.program;
    uint32_t low = 0, high = p->groups.count;
    Primary out = {.rank = ce->ranks[0]};

    if (ce->ranks[0] == s->separator)
        return out;
    if (ce->root_primary >= p->root.metadata.generic_limit) {
        out.band = 2;
        return out;
    }

    while (low < high) {
        uint32_t mid = low + (high - low) / 2;

        if (qjs_intl_read_u32(p->groups.bytes + (size_t)mid * 8) <= ce->root_primary)
            low = mid + 1;
        else
            high = mid;
    }
    if (low) {
        out.band = 1;
        out.group = qjs_intl_read_u32(p->group_ranks.bytes + (size_t)(low - 1) * 4);
    }

    return out;
}

static void stream_clear(Stream *s)
{
    const QJSIntlAllocator *a = &s->iterator.match.allocator;

    if (s->elements && s->elements != s->small)
        a->free(a->opaque, s->elements);
    qjs_intl_collation_ranked_elements_clear(&s->iterator);
}

static QJSIntlStatus retain(Stream *s, const QJSIntlCollationRankedElement *ce, int shifted)
{
    const QJSIntlAllocator *a = &s->iterator.match.allocator;

    if (!s->retain)
        return QJS_INTL_OK;

    if (s->count == s->capacity) {
        SavedElement *next;
        size_t capacity;

        if (s->capacity > SIZE_MAX / sizeof(*next) / 2)
            return QJS_INTL_OVERFLOW;
        capacity = s->capacity * 2;
        if (s->elements == s->small) {
            next = a->malloc(a->opaque, capacity * sizeof(*next));
            if (next)
                memcpy(next, s->small, s->count * sizeof(*next));
        } else {
            next = a->realloc(a->opaque, s->elements, capacity * sizeof(*next));
        }
        if (!next)
            return QJS_INTL_NO_MEMORY;
        s->elements = next;
        s->capacity = capacity;
    }

    s->elements[s->count++] = (SavedElement){*ce, shifted};
    return QJS_INTL_OK;
}

static QJSIntlStatus next_primary(Stream *s, Primary *out)
{
    QJSIntlStatus status;

    memset(out, 0, sizeof(*out));
    for (;;) {
        QJSIntlCollationRankedToken token;
        int present;

        status = qjs_intl_collation_ranked_elements_next(&s->iterator, &token, &present);
        if (status || !present)
            return status;

        if (token.number.length) {
            token.element.ranks[0] = s->numeric;
            token.element.ranks[1] = s->common_s;
            token.element.ranks[2] = s->common_t;
            token.element.root_primary = s->iterator.program.root.metadata.numeric;
        }

        /* Wholly ignorable CEs have no weight or alignment on any level.
           Keep them out of the weak buffer without merging numeric runs. */
        if (!token.element.ranks[0] && !token.element.ranks[1] &&
            !token.element.ranks[2] && !token.element.ranks[3])
            continue;

        if (s->shifted) {
            const QJSIntlCollationRankedElement *ce = &token.element;
            int variable = ce->ranks[0] && ce->ranks[0] != s->separator &&
                ce->root_primary > s->iterator.program.root.metadata.field_separator &&
                ce->root_primary < s->variable_limit;

            if (variable) {
                s->variable_run = 1;
                s->any_quaternary = 1;
                status = s->retain_quaternary ? retain(s, ce, 1) : QJS_INTL_OK;
                if (status)
                    return status;
                continue;
            }
            if (!ce->ranks[0] && s->variable_run)
                continue; /* Following primary-ignorables vanish through Q. */
            if (ce->ranks[0])
                s->variable_run = 0;
        }

        if (token.element.ranks[3])
            s->any_quaternary = 1;
        status = retain(s, &token.element, 0);
        if (status)
            return status;
        if (token.element.ranks[0]) {
            *out = primary(s, &token.element);
            out->number = token.number;
            return QJS_INTL_OK;
        }
    }
}

static int compare_primary_key(const Primary *a, const Primary *b)
{
    int order = compare_u32(a->band, b->band);

    if (!order)
        order = compare_u32(a->group, b->group);
    if (!order)
        order = compare_u32(a->rank, b->rank);

    return order;
}

static int compare_primary(const Primary *a, const Primary *b)
{
    uint32_t i;
    int order = compare_primary_key(a, b);

    if (order || !a->number.length)
        return order;

    order = compare_u32(a->number.length, b->number.length);
    if (order)
        return order;
    for (i = 0; i < a->number.length; i++) {
        int x = lre_decimal_value(a->number.digits[i] & QJS_INTL_COLLATION_SCALAR_MASK);
        int y = lre_decimal_value(b->number.digits[i] & QJS_INTL_COLLATION_SCALAR_MASK);

        if (x != y)
            return x < y ? -1 : 1;
    }

    return 0;
}

enum { SECONDARY, CASE, TERTIARY };

typedef struct WeakWeight {
    uint32_t case_weight, rank;
    int present;
} WeakWeight;

static WeakWeight next_weak(const Stream *s, size_t *at, unsigned int level,
                            const QJSIntlCollationPolicy *policy)
{
    while (*at < s->count) {
        const SavedElement *saved = s->elements + (*at)++;
        const QJSIntlCollationRankedElement *ce = &saved->ce;
        WeakWeight weight = {0};

        if (saved->shifted)
            continue;

        if (level == SECONDARY) {
            weight.rank = ce->ranks[1];
        } else if (level == CASE) {
            if (!ce->ranks[0] && (policy->strength == 1 || !ce->ranks[1]))
                continue;
            weight.rank = ce->case_value + 1;
            if (policy->case_first == QJS_INTL_COLLATION_UPPER_FIRST)
                weight.rank = 4 - weight.rank;
        } else {
            weight.rank = ce->ranks[2];
            if (weight.rank && !policy->case_level && policy->case_first != QJS_INTL_COLLATION_CASE_OFF) {
                weight.case_weight = ce->case_value + 1;
                if (policy->case_first == QJS_INTL_COLLATION_UPPER_FIRST)
                    weight.case_weight = 4 - weight.case_weight;
                /* T-only elements must remain above every ordinary combined
                   case/T value even under upper-first (TR35 WF2). */
                if (!ce->ranks[0] && !ce->ranks[1])
                    weight.case_weight = 3;
            }
        }

        if (weight.rank) {
            weight.present = 1;
            return weight;
        }
    }

    return (WeakWeight){0};
}

static int compare_weak(const Stream *a, const Stream *b, unsigned int level,
                        const QJSIntlCollationPolicy *policy)
{
    size_t left = 0, right = 0;

    for (;;) {
        WeakWeight x = next_weak(a, &left, level, policy), y = next_weak(b, &right, level, policy);
        int order = compare_u32(x.present, y.present);

        if (!order)
            order = compare_u32(x.case_weight, y.case_weight);
        if (!order)
            order = compare_u32(x.rank, y.rank);
        if (order || !x.present)
            return order;
    }
}

typedef struct Quaternary {
    unsigned int band; /* End, shifted variable, regular/common. */
    Primary primary;
    uint32_t rank;
} Quaternary;

static Quaternary next_quaternary(const Stream *s, size_t *at)
{
    while (*at < s->count) {
        const SavedElement *saved = s->elements + (*at)++;
        const QJSIntlCollationRankedElement *ce = &saved->ce;

        if (saved->shifted)
            return (Quaternary){.band = 1, .primary = primary(s, ce)};

        /* Q=0 on a regular CE is a common alignment marker, not a weight
           to discard when another CE has an explicit Q distinction. Q-only
           CEs share the same ordered Q domain; wholly zero CEs were omitted. */
        return (Quaternary){.band = 2, .rank = ce->ranks[3]};
    }

    return (Quaternary){0};
}

static int compare_quaternary(const Stream *a, const Stream *b)
{
    size_t left = 0, right = 0;

    /* Common markers alone must not invent a difference from CE counts. */
    if (!a->any_quaternary && !b->any_quaternary)
        return 0;

    for (;;) {
        Quaternary x = next_quaternary(a, &left), y = next_quaternary(b, &right);
        int order = compare_u32(x.band, y.band);

        if (!order && x.band == 1)
            order = compare_primary_key(&x.primary, &y.primary);
        if (!order && x.band == 2)
            order = compare_u32(x.rank, y.rank);
        if (order || !x.band)
            return order;
    }
}

QJSIntlStatus qjs_intl_collation_compare_ranked(const QJSIntlCompiledCollationProgram *program,
                                                const uint16_t *left, size_t left_length,
                                                const uint16_t *right, size_t right_length,
                                                const QJSIntlAllocator *allocator,
                                                const QJSIntlCollationPolicy *options, int *order)
{
    Stream streams[2] = {0};
    QJSIntlCollationPolicy policy;
    QJSIntlStatus status = QJS_INTL_OK;
    unsigned int i;

    if (order)
        *order = 0;
    if (!program || !program->ready || !order || !allocator || !allocator->malloc ||
        !allocator->realloc || !allocator->free || (!left && left_length) || (!right && right_length))
        return QJS_INTL_INVALID_ARGUMENT;

    policy = options ? *options : program->policy;
    if ((unsigned int)policy.strength < 1 || (unsigned int)policy.strength > 5 ||
        (unsigned int)policy.case_first > QJS_INTL_COLLATION_LOWER_FIRST ||
        (unsigned int)policy.max_variable > QJS_INTL_COLLATION_VARIABLE_CURRENCY)
        return QJS_INTL_INVALID_ARGUMENT;
    if (policy.strength > 4 || policy.backwards_secondary)
        return QJS_INTL_UNSUPPORTED;
    if (left_length > SIZE_MAX / sizeof(*left) || right_length > SIZE_MAX / sizeof(*right))
        return QJS_INTL_OVERFLOW;

    /* Only exact identity can skip matching safely. A common text prefix can
       cross a contraction or alter the remaining string's preceding context. */
    if (left_length == right_length && (!left_length || !memcmp(left, right, left_length * sizeof(*left))))
        return QJS_INTL_OK;

    status = qjs_intl_collation_ranked_elements_init(&streams[0].iterator, program,
                                                   left, left_length, policy.numeric, allocator);
    if (!status)
        status = qjs_intl_collation_ranked_elements_init(&streams[1].iterator, program,
                                                       right, right_length, policy.numeric, allocator);
    if (status)
        goto done;

    for (i = 0; i < 2; i++) {
        Stream *s = streams + i;
        const QJSIntlCollationBaseMetadata *m = &program->root.metadata;

        s->elements = s->small;
        s->capacity = sizeof(s->small) / sizeof(*s->small);
        s->retain = policy.strength > 1 || policy.case_level;
        s->retain_quaternary = policy.strength >= 4;
        s->shifted = !!policy.shifted;
        s->variable_limit = qjs_intl_read_u32(program->groups.bytes +
            (size_t)(program->core_groups[policy.max_variable] + 1) * 8);
        qjs_intl_compiled_collation_root_rank(program, 0, m->numeric, &s->numeric);
        qjs_intl_compiled_collation_root_rank(program, 0, m->field_separator, &s->separator);
        qjs_intl_compiled_collation_root_rank(program, 1, m->limits.secondary_common, &s->common_s);
        qjs_intl_compiled_collation_root_rank(program, 2, m->limits.tertiary_common, &s->common_t);
    }

    for (;;) {
        Primary a, b;

        status = next_primary(streams, &a);
        if (!status)
            status = next_primary(streams + 1, &b);
        if (status)
            goto done;
        *order = compare_primary(&a, &b);
        if (*order || !a.rank)
            goto levels;
    }

levels:
    if (!*order && policy.strength >= 2)
        *order = compare_weak(streams, streams + 1, SECONDARY, &policy);
    if (!*order && policy.case_level)
        *order = compare_weak(streams, streams + 1, CASE, &policy);
    if (!*order && policy.strength >= 3)
        *order = compare_weak(streams, streams + 1, TERTIARY, &policy);
    if (!*order && policy.strength >= 4)
        *order = compare_quaternary(streams, streams + 1);

done:
    stream_clear(streams);
    stream_clear(streams + 1);
    return status;
}
