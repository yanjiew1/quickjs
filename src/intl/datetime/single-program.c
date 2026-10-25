/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "single-program.h"
#include <string.h>

typedef struct Choice {
    size_t index;
    unsigned int family, width;
    int found;
} Choice;

typedef struct Pair {
    Choice normal, twelve;
    int eligible;
} Pair;

static int hour12(unsigned int symbol)
{
    return symbol == 'h' || symbol == 'K';
}

static int same_components(const QJSIntlDateTimeComponents *a, const QJSIntlDateTimeComponents *b,
                           int skip_hour)
{
    size_t i;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        if ((!skip_hour || i != QJS_INTL_DATETIME_HOUR) && a->values[i] != b->values[i])
            return 0;
    }
    return 1;
}

/* Public year metadata groups numeric and cyclic representations together.
   Prefer an actual numeric-year source when possible; never relabel U as r. */
static int cyclic_only(unsigned int kinds)
{
    return kinds == QJS_INTL_DATE_YEAR_CYCLIC;
}

static unsigned int period_class(const QJSIntlDatePatternInfo *info)
{
    return info->period_symbol == 'b' ? 1 : info->period_symbol == 'B' ? 2 : 0;
}

static void consider(Choice *choice, size_t index, unsigned int family, unsigned int width)
{
    if (!choice->found || family < choice->family ||
        (family == choice->family && (width < choice->width ||
                                     (width == choice->width && index < choice->index))))
        *choice = (Choice){index, family, width, 1};
}

static int better_pair(const Pair *a, const Pair *b)
{
    unsigned int family_a = a->normal.family + a->twelve.family;
    unsigned int family_b = b->normal.family + b->twelve.family;
    unsigned int width_a = a->normal.width + a->twelve.width;
    unsigned int width_b = b->normal.width + b->twelve.width;
    size_t first_a = a->normal.index < a->twelve.index ? a->normal.index : a->twelve.index;
    size_t first_b = b->normal.index < b->twelve.index ? b->normal.index : b->twelve.index;
    if (family_a != family_b)
        return family_a < family_b;
    if (width_a != width_b)
        return width_a < width_b;
    if (first_a != first_b)
        return first_a < first_b;
    if (a->normal.index != b->normal.index)
        return a->normal.index < b->normal.index;
    return a->twelve.index < b->twelve.index;
}

static QJSIntlStatus copy_program(const QJSIntlDateSourceCandidates *pool, const Choice *choice,
                                  const QJSIntlDateTimeComponents *components, int twelve,
                                  const QJSIntlAllocator *allocator, QJSIntlDateSingleProgram *out)
{
    const QJSIntlDateSourceCandidate *source = &pool->records[choice->index];
    out->source_info = source->info;
    out->source_index = choice->index;
    return qjs_intl_date_program_adapt(allocator, &source->pattern, components,
                                      QJS_INTL_DATE_ADAPT_HOUR_WIDTH, twelve,
                                      &out->pattern, &out->adaptations);
}

void qjs_intl_date_single_programs_clear(QJSIntlDateSinglePrograms *programs)
{
    if (!programs)
        return;
    qjs_intl_date_pattern_clear(&programs->allocator, &programs->normal.pattern);
    qjs_intl_date_pattern_clear(&programs->allocator, &programs->twelve.pattern);
    memset(programs, 0, sizeof(*programs));
}

QJSIntlStatus qjs_intl_date_single_programs_open(const QJSIntlDateSourceCandidates *pool,
                                               const QJSIntlDateFormatSources *groups,
                                               size_t group, const QJSIntlAllocator *allocator,
                                               QJSIntlDateSinglePrograms *out)
{
    QJSIntlDateSinglePrograms result = {0};
    Pair classes[8][3] = {0};
    Pair *chosen = NULL;
    QJSIntlStatus status;
    size_t i, count;
    unsigned int year, period;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !pool || !groups || !pool->records || pool->count != groups->source_count ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_format_source_group(groups, group, &result.components, &count);
    if (status)
        return status;
    result.allocator = *allocator;
    if (!result.components.values[QJS_INTL_DATETIME_HOUR]) {
        Choice choice = {0};
        int rank = 2;
        for (i = 0; i < count; i++) {
            size_t index;
            int candidate;
            status = qjs_intl_date_format_source_member(groups, group, i, &index);
            if (status || index >= pool->count)
                return QJS_INTL_INVALID_ARGUMENT;
            candidate = cyclic_only(pool->records[index].info.year_kinds);
            if (candidate < rank) {
                rank = candidate;
                choice.index = index;
            }
        }
        status = copy_program(pool, &choice, &result.components, 0, allocator, &result.normal);
        if (status)
            goto fail;
        *out = result;
        return QJS_INTL_OK;
    }
    /* Eligible year/period classes must occur in the group itself. Keep the
       best whole source for each family in a fixed-size class table, avoiding
       candidate-pair enumeration or discarding an unpairable first class. */
    for (i = 0; i < count; i++) {
        size_t index;
        const QJSIntlDatePatternInfo *info;
        status = qjs_intl_date_format_source_member(groups, group, i, &index);
        if (status || index >= pool->count)
            return QJS_INTL_INVALID_ARGUMENT;
        info = &pool->records[index].info;
        if (info->year_kinds >= 8)
            return QJS_INTL_DATA_ERROR;
        classes[info->year_kinds][period_class(info)].eligible = 1;
    }
    for (i = 0; i < pool->count; i++) {
        const QJSIntlDatePatternInfo *info = &pool->records[i].info;
        Pair *pair;
        unsigned int width;
        if (info->year_kinds >= 8)
            return QJS_INTL_DATA_ERROR;
        pair = &classes[info->year_kinds][period_class(info)];
        if (!pair->eligible || !info->hour_symbol || !same_components(&info->components, &result.components, 1))
            continue;
        width = info->components.values[QJS_INTL_DATETIME_HOUR] != result.components.values[QJS_INTL_DATETIME_HOUR];
        if (info->period_symbol != 'a')
            consider(&pair->normal, i, hour12(info->hour_symbol), width);
        if (info->period_symbol == 'a' || info->period_symbol == 'b' || info->period_symbol == 'B')
            consider(&pair->twelve, i, !hour12(info->hour_symbol), width);
    }
    for (year = 0; year < 8; year++) {
        for (period = 0; period < 3; period++) {
            Pair *pair = &classes[year][period];
            if (pair->normal.found && pair->twelve.found) {
                int rank = cyclic_only(year);
                int previous = chosen ? cyclic_only(pool->records[chosen->normal.index].info.year_kinds) : 2;
                if (rank < previous || (rank == previous && better_pair(pair, chosen)))
                    chosen = pair;
            }
        }
    }
    if (!chosen)
        return QJS_INTL_UNSUPPORTED;
    status = copy_program(pool, &chosen->normal, &result.components, 0, allocator, &result.normal);
    if (!status)
        status = copy_program(pool, &chosen->twelve, &result.components, 1, allocator, &result.twelve);
    if (status)
        goto fail;
    *out = result;
    return QJS_INTL_OK;
fail:
    qjs_intl_date_single_programs_clear(&result);
    return status;
}
