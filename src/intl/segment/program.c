/* Checked segmentation program views; execution/refinement lives elsewhere.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "program.h"
#include "../native/wire.h"
#include <string.h>

enum {
    HEADER = 72, DESCRIPTOR = 40, BINDING = 12, RULE = 8, MACHINE = 16,
    MAX_SIZE = 32 * 1024 * 1024, MAX_PROGRAMS = 64, MAX_BINDINGS = 65536,
    MAX_CLASSES = 16384, MAX_SPANS = 262144, MAX_RULES = 1024, MAX_MACHINES = 2048,
    MAX_STATES = 4096, MAX_CELLS = 1048576,
    MAX_TOTAL_STATES = 262144, MAX_TOTAL_CELLS = 8388608
};

typedef struct Totals {
    uint32_t spans, states, cells;
} Totals;

static uint32_t word(const uint8_t *array, uint32_t index)
{
    return qjs_intl_read_u32(array + (size_t)index * 4);
}

static uint32_t half(const uint8_t *array, uint32_t index)
{
    const uint8_t *p = array + (size_t)index * 2;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static QJSIntlStatus array(const uint8_t *bytes, size_t length, size_t *at,
                           uint32_t count, size_t width, const uint8_t **out)
{
    size_t padding = (4 - (*at & 3)) & 3;
    if (*at > length || padding > length - *at)
        return QJS_INTL_DATA_ERROR;
    while (padding) {
        if (bytes[(*at)++])
            return QJS_INTL_DATA_ERROR;
        padding--;
    }
    if (count > (length - *at) / width)
        return QJS_INTL_DATA_ERROR;
    *out = bytes + *at;
    *at += count * width;
    return QJS_INTL_OK;
}

static QJSIntlStatus program_view(const QJSIntlSection *section, const uint8_t *descriptor,
                                  QJSIntlSegmentProgram *out)
{
    static const uint32_t requirements[] = {0, 13, 10};
    uint32_t start = word(descriptor, 6), length = word(descriptor, 7);
    size_t at = 0;
    QJSIntlStatus status;
    out->kind = word(descriptor, 0);
    out->requirements = word(descriptor, 1);
    out->class_count = word(descriptor, 2);
    out->range_count = word(descriptor, 3);
    out->rule_count = word(descriptor, 4);
    out->machine_count = word(descriptor, 5);
    if (out->kind > 2 || out->requirements != requirements[out->kind] ||
        !out->class_count || out->class_count > MAX_CLASSES ||
        !out->range_count || out->range_count > MAX_SPANS ||
        out->rule_count > MAX_RULES || out->machine_count > MAX_MACHINES ||
        out->machine_count > 2 * out->rule_count ||
        (!!out->rule_count != !!out->machine_count) || word(descriptor, 8) || word(descriptor, 9) ||
        start > section->length || length > section->length - start)
        return QJS_INTL_DATA_ERROR;
    out->bytes = section->bytes + start;
    out->length = length;
    status = array(out->bytes, length, &at, out->range_count, 4, &out->ends);
    if (!status)
        status = array(out->bytes, length, &at, out->range_count, 2, &out->classes);
    if (!status)
        status = array(out->bytes, length, &at, out->class_count, 1, &out->flags);
    if (!status)
        status = array(out->bytes, length, &at, out->rule_count, RULE, &out->rules);
    if (!status)
        status = array(out->bytes, length, &at, out->machine_count, MACHINE, &out->machines);
    return status;
}

static void machine_view(const QJSIntlSegmentProgram *program, uint32_t index,
                         QJSIntlSegmentMachine *out)
{
    const uint8_t *descriptor = program->machines + (size_t)index * MACHINE;
    out->state_count = word(descriptor, 0);
    out->mode = word(descriptor, 1);
    out->class_count = program->class_count;
    out->transitions = program->bytes + word(descriptor, 2);
    out->accepts = program->bytes + word(descriptor, 3);
}

static QJSIntlStatus validate_program(const QJSIntlSegmentProgram *program, Totals *totals)
{
    uint32_t i, previous = 0, prior_class = UINT32_MAX, used = 0;
    size_t at = (size_t)(program->machines - program->bytes) + (size_t)program->machine_count * MACHINE;
    const uint8_t *unused;
    QJSIntlStatus status;
    if (program->range_count > MAX_SPANS - totals->spans)
        return QJS_INTL_DATA_ERROR;
    totals->spans += program->range_count;
    /* Establish every machine extent and aggregate product before graph
       traversal. No pointer is formed from an unvalidated body offset. */
    for (i = 0; i < program->machine_count; i++) {
        const uint8_t *descriptor = program->machines + (size_t)i * MACHINE;
        uint32_t states = word(descriptor, 0), mode = word(descriptor, 1), cells;
        const uint8_t *transitions, *accepts;
        if (!states || states > MAX_STATES || mode > QJS_INTL_SEGMENT_LEFT_BOS_ONLY ||
            states > MAX_CELLS / program->class_count)
            return QJS_INTL_DATA_ERROR;
        cells = states * program->class_count;
        if (states > MAX_TOTAL_STATES - totals->states || cells > MAX_TOTAL_CELLS - totals->cells)
            return QJS_INTL_DATA_ERROR;
        totals->states += states;
        totals->cells += cells;
        status = array(program->bytes, program->length, &at, cells, 2, &transitions);
        if (!status)
            status = array(program->bytes, program->length, &at, (states + 7) / 8, 1, &accepts);
        if (status)
            return status;
        if (word(descriptor, 2) != (size_t)(transitions - program->bytes) ||
            word(descriptor, 3) != (size_t)(accepts - program->bytes) ||
            ((states & 7) && (accepts[(states - 1) / 8] >> (states & 7))))
            return QJS_INTL_DATA_ERROR;
    }
    status = array(program->bytes, program->length, &at, 0, 1, &unused);
    if (status || at != program->length)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < program->range_count; i++) {
        uint32_t end = word(program->ends, i), identifier = half(program->classes, i);
        if (end <= previous || end > 0x110000 || identifier >= program->class_count ||
            identifier == prior_class || identifier > used)
            return QJS_INTL_DATA_ERROR;
        if (identifier == used)
            used++;
        previous = end;
        prior_class = identifier;
    }
    if (previous != 0x110000 || used != program->class_count)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < program->class_count; i++) {
        unsigned int flag = program->flags[i];
        if ((program->kind != QJS_INTL_SEGMENT_SENTENCE && flag) || flag > 16 || (flag & (flag - 1)))
            return QJS_INTL_DATA_ERROR;
    }
    used = 0;
    for (i = 0; i < program->rule_count; i++) {
        const uint8_t *rule = program->rules + (size_t)i * RULE;
        uint32_t left = half(rule, 0), right = half(rule, 1), side;
        unsigned int breaks = rule[4], role = rule[5];
        if (left >= program->machine_count || right >= program->machine_count ||
            breaks > 1 || role > 2 || half(rule, 3) ||
            (role == QJS_INTL_SEGMENT_LEXICAL && (program->kind != QJS_INTL_SEGMENT_WORD || breaks)) ||
            (role == QJS_INTL_SEGMENT_SOFT_SENTENCE && (program->kind != QJS_INTL_SEGMENT_SENTENCE || !breaks)) ||
            word(program->machines + (size_t)left * MACHINE, 1) == QJS_INTL_SEGMENT_RIGHT_REVERSED_RESTART ||
            word(program->machines + (size_t)right * MACHINE, 1) != QJS_INTL_SEGMENT_RIGHT_REVERSED_RESTART)
            return QJS_INTL_DATA_ERROR;
        for (side = 0; side < 2; side++) {
            uint32_t index = side ? right : left;
            if (index > used)
                return QJS_INTL_DATA_ERROR;
            if (index == used)
                used++;
        }
    }
    if (used != program->machine_count)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < program->machine_count; i++) {
        QJSIntlSegmentMachine machine;
        uint32_t state, reached = 1;
        machine_view(program, i, &machine);
        /* Compiler BFS numbering proves reachability without scratch. Unlike
           dictionary DAGs, cycles and wholly rejecting machines are valid. */
        for (state = 0; state < machine.state_count; state++) {
            uint32_t character_class;
            if (state >= reached)
                return QJS_INTL_DATA_ERROR;
            for (character_class = 0; character_class < machine.class_count; character_class++) {
                uint32_t target = half(machine.transitions, state * machine.class_count + character_class);
                if (target >= machine.state_count || target > reached)
                    return QJS_INTL_DATA_ERROR;
                if (target == reached)
                    reached++;
            }
        }
        if (reached != machine.state_count)
            return QJS_INTL_DATA_ERROR;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_program_data_open(const QJSIntlData *shared, QJSIntlSegmentProgramData *out)
{
    QJSIntlSegmentProgramData result = {0};
    Totals totals = {0};
    QJSIntlStatus status;
    const uint8_t *header;
    uint32_t i, used = 0, previous_locale = 0, previous_kind = 0;
    size_t at;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!shared || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_pack_section(&shared->pack, QJS_INTL_SECTION_SEGMENT_PROGRAMS, &result.section);
    if (status)
        return status;
    if (result.section.length < HEADER || result.section.length > MAX_SIZE)
        return QJS_INTL_DATA_ERROR;
    header = result.section.bytes;
    if (memcmp(header, "QJSSEGM\0", 8))
        return QJS_INTL_DATA_ERROR;
    if (word(header, 2) != 1 || word(header, 3) != (18u << 16) || word(header, 4) != 1)
        return QJS_INTL_UNSUPPORTED;
    result.program_count = word(header, 5);
    result.binding_count = word(header, 6);
    result.locale_count = shared->locales.count;
    result.fingerprint = header + 40;
    if (!result.program_count || result.program_count > MAX_PROGRAMS ||
        result.program_count != result.section.count || !result.binding_count ||
        result.binding_count > MAX_BINDINGS || word(header, 7) != result.section.length ||
        word(header, 8) || word(header, 9))
        return QJS_INTL_DATA_ERROR;
    at = HEADER;
    status = array(header, result.section.length, &at, result.program_count, DESCRIPTOR, &result.descriptors);
    if (!status)
        status = array(header, result.section.length, &at, result.binding_count, BINDING, &result.bindings);
    if (status)
        return status;
    for (i = 0; i < result.program_count; i++) {
        const uint8_t *descriptor = result.descriptors + (size_t)i * DESCRIPTOR;
        QJSIntlSegmentProgram program = {0};
        if (word(descriptor, 6) != at)
            return QJS_INTL_DATA_ERROR;
        status = program_view(&result.section, descriptor, &program);
        if (!status)
            status = validate_program(&program, &totals);
        if (status)
            return status;
        at += program.length;
    }
    if (at != result.section.length)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < result.binding_count; i++) {
        const uint8_t *binding = result.bindings + (size_t)i * BINDING;
        uint32_t locale = word(binding, 0), kind = word(binding, 1), index = word(binding, 2);
        if (locale >= result.locale_count || kind > 2 || index >= result.program_count || index > used ||
            (i && (locale < previous_locale || (locale == previous_locale && kind <= previous_kind))) ||
            kind != word(result.descriptors + (size_t)index * DESCRIPTOR, 0))
            return QJS_INTL_DATA_ERROR;
        if (index == used)
            used++;
        previous_locale = locale;
        previous_kind = kind;
    }
    if (used != result.program_count)
        return QJS_INTL_DATA_ERROR;
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_program_at(const QJSIntlSegmentProgramData *data, uint32_t index,
                                          QJSIntlSegmentProgram *out)
{
    QJSIntlSegmentProgram result = {0};
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!data || !data->section.bytes || !out || index >= data->program_count)
        return QJS_INTL_INVALID_ARGUMENT;
    status = program_view(&data->section, data->descriptors + (size_t)index * DESCRIPTOR, &result);
    if (!status)
        *out = result;
    return status;
}

QJSIntlStatus qjs_intl_segment_program_find(const QJSIntlSegmentProgramData *data, uint32_t locale,
                                            QJSIntlGranularity kind, QJSIntlSegmentProgram *out)
{
    uint32_t low = 0, high;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!data || !data->section.bytes || !out || locale >= data->locale_count || (unsigned int)kind > 2)
        return QJS_INTL_INVALID_ARGUMENT;
    high = data->binding_count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t *binding = data->bindings + (size_t)middle * BINDING;
        uint32_t found_locale = word(binding, 0), found_kind = word(binding, 1);
        if (found_locale < locale || (found_locale == locale && found_kind < (unsigned int)kind))
            low = middle + 1;
        else if (found_locale == locale && found_kind == (unsigned int)kind)
            return qjs_intl_segment_program_at(data, word(binding, 2), out);
        else
            high = middle;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_segment_program_classify(const QJSIntlSegmentProgram *program, uint32_t code, uint32_t *out)
{
    uint32_t low = 0, high;
    if (out)
        *out = 0;
    if (!program || !program->ends || !out || code > 0x10ffff)
        return QJS_INTL_INVALID_ARGUMENT;
    high = program->range_count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        if (code >= word(program->ends, middle))
            low = middle + 1;
        else
            high = middle;
    }
    *out = half(program->classes, low);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_program_rule(const QJSIntlSegmentProgram *program,
                                            uint32_t index, QJSIntlSegmentRule *out)
{
    const uint8_t *rule;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!program || !program->rules || !out || index >= program->rule_count)
        return QJS_INTL_INVALID_ARGUMENT;
    rule = program->rules + (size_t)index * RULE;
    out->left = half(rule, 0);
    out->right = half(rule, 1);
    out->breaks = rule[4];
    out->role = rule[5];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_program_machine(const QJSIntlSegmentProgram *program,
                                               uint32_t index, QJSIntlSegmentMachine *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!program || !program->machines || !out || index >= program->machine_count)
        return QJS_INTL_INVALID_ARGUMENT;
    machine_view(program, index, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_machine_step(const QJSIntlSegmentMachine *machine, uint32_t state,
                                            uint32_t character_class, uint32_t *out)
{
    if (out)
        *out = 0;
    if (!machine || !machine->transitions || !out || state >= machine->state_count || character_class >= machine->class_count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = half(machine->transitions, state * machine->class_count + character_class);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_machine_accepts(const QJSIntlSegmentMachine *machine, uint32_t state, int *out)
{
    if (out)
        *out = 0;
    if (!machine || !machine->accepts || !out || state >= machine->state_count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = (machine->accepts[state / 8] >> (state & 7)) & 1;
    return QJS_INTL_OK;
}
