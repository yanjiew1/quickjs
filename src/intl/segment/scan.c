/* Two directional DFA passes over one decoded input; no native admission here.
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
#include "scan.h"
#include "../../unicode/utf16.h"
#include <string.h>

typedef struct MachineRun {
    QJSIntlSegmentMachine table;
    uint32_t state;
} MachineRun;

static int valid_allocator(const QJSIntlAllocator *allocator)
{
    return allocator && allocator->malloc && allocator->free;
}

static QJSIntlStatus reserve(size_t *size, size_t count, size_t width, size_t *offset)
{
    if (count > (SIZE_MAX - *size) / width)
        return QJS_INTL_OVERFLOW;
    *offset = *size;
    *size += count * width;
    return QJS_INTL_OK;
}

void qjs_intl_segment_text_clear(QJSIntlSegmentText *text)
{
    if (text) {
        if (text->storage)
            text->allocator.free(text->allocator.opaque, text->storage);
        memset(text, 0, sizeof(*text));
    }
}

QJSIntlStatus qjs_intl_segment_text_prepare(const QJSIntlAllocator *allocator, const QJSIntlUTF16 *input,
                                            QJSIntlSegmentText *out)
{
    QJSIntlSegmentText result = {0};
    size_t at = 0, size = 0, offsets_at, scalars_at, i;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !valid_allocator(allocator) || !input || (!input->data && input->length) ||
        input->length > SIZE_MAX / sizeof(*input->data))
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = *allocator;
    result.length = input->length;
    while (at < input->length) {
        unicode_utf16_next(input->data, input->length, &at);
        result.count++;
    }
    status = reserve(&size, result.count + 1, sizeof(size_t), &offsets_at);
    if (!status)
        status = reserve(&size, result.count, sizeof(uint32_t), &scalars_at);
    if (status)
        return status;
    result.storage = allocator->malloc(allocator->opaque, size);
    if (!result.storage)
        return QJS_INTL_NO_MEMORY;
    result.offsets = (size_t *)((uint8_t *)result.storage + offsets_at);
    result.scalars = (uint32_t *)((uint8_t *)result.storage + scalars_at);
    at = 0;
    for (i = 0; i < result.count; i++) {
        result.offsets[i] = at;
        result.scalars[i] = unicode_utf16_next(input->data, input->length, &at);
    }
    result.offsets[result.count] = at;
    *out = result;
    return QJS_INTL_OK;
}

void qjs_intl_segment_scan_clear(QJSIntlSegmentScan *scan)
{
    if (scan) {
        if (scan->storage)
            scan->allocator.free(scan->allocator.opaque, scan->storage);
        memset(scan, 0, sizeof(*scan));
    }
}

static unsigned int accepts(const MachineRun *machine)
{
    return (machine->table.accepts[machine->state / 8] >> (machine->state & 7)) & 1;
}

static void step(MachineRun *machine, uint16_t character_class)
{
    const uint8_t *p = machine->table.transitions +
        ((size_t)machine->state * machine->table.class_count + character_class) * 2;
    machine->state = (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

QJSIntlStatus qjs_intl_segment_scan_prepare(const QJSIntlAllocator *allocator,
                                            const QJSIntlSegmentProgram *program,
                                            const QJSIntlSegmentText *text, QJSIntlSegmentScan *out)
{
    QJSIntlSegmentScan result = {0};
    MachineRun *machines = NULL;
    QJSIntlSegmentRule *rules = NULL;
    uint16_t *mapping = NULL;
    uint8_t *right_masks = NULL;
    void *scratch = NULL;
    uint32_t i, left_count = 0, right_count = 0, left_at = 0, right_at;
    size_t size = 0, classes_at, rules_at, breaks_at, scratch_size = 0;
    size_t machine_at, rule_at, mapping_at, masks_at, stride, position;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !valid_allocator(allocator) || !program || !program->bytes ||
        !text || !text->storage || !text->offsets || !text->scalars ||
        text->length > SIZE_MAX / sizeof(uint16_t) || text->count > text->length)
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = *allocator;
    result.count = text->count;
    status = reserve(&size, text->count, sizeof(uint16_t), &classes_at);
    if (!status)
        status = reserve(&size, text->count + 1, sizeof(uint16_t), &rules_at);
    if (!status)
        status = reserve(&size, text->count + 1, sizeof(uint8_t), &breaks_at);
    if (status)
        return status;
    result.storage = allocator->malloc(allocator->opaque, size);
    if (!result.storage)
        return QJS_INTL_NO_MEMORY;
    result.classes = (uint16_t *)((uint8_t *)result.storage + classes_at);
    result.rules = (uint16_t *)((uint8_t *)result.storage + rules_at);
    result.breaks = (uint8_t *)result.storage + breaks_at;
    for (position = 0; position < text->count; position++) {
        uint32_t character_class;
        status = qjs_intl_segment_program_classify(program, text->scalars[position], &character_class);
        if (status)
            goto done;
        result.classes[position] = character_class;
    }
    for (i = 0; i < program->machine_count; i++) {
        QJSIntlSegmentMachine machine;
        status = qjs_intl_segment_program_machine(program, i, &machine);
        if (status)
            goto done;
        if (machine.mode == QJS_INTL_SEGMENT_RIGHT_REVERSED_RESTART)
            right_count++;
        else
            left_count++;
    }
    stride = (right_count + 7) / 8;
    status = reserve(&scratch_size, program->machine_count, sizeof(MachineRun), &machine_at);
    if (!status)
        status = reserve(&scratch_size, program->rule_count, sizeof(QJSIntlSegmentRule), &rule_at);
    if (!status)
        status = reserve(&scratch_size, program->machine_count, sizeof(uint16_t), &mapping_at);
    if (!status)
        status = reserve(&scratch_size, right_count ? text->count + 1 : 0, stride ? stride : 1, &masks_at);
    if (status)
        goto done;
    /* A default-only program needs no machine scratch or acceptance matrix. */
    if (program->machine_count) {
        scratch = allocator->malloc(allocator->opaque, scratch_size);
        if (!scratch) {
            status = QJS_INTL_NO_MEMORY;
            goto done;
        }
        machines = (MachineRun *)((uint8_t *)scratch + machine_at);
        rules = (QJSIntlSegmentRule *)((uint8_t *)scratch + rule_at);
        mapping = (uint16_t *)((uint8_t *)scratch + mapping_at);
        right_masks = (uint8_t *)scratch + masks_at;
        memset(right_masks, 0, (text->count + 1) * stride);
    }
    right_at = left_count;
    for (i = 0; i < program->machine_count; i++) {
        QJSIntlSegmentMachine machine;
        uint32_t index;
        status = qjs_intl_segment_program_machine(program, i, &machine);
        if (status)
            goto done;
        index = machine.mode == QJS_INTL_SEGMENT_RIGHT_REVERSED_RESTART ? right_at++ : left_at++;
        machines[index].table = machine;
        machines[index].state = 0;
        mapping[i] = index;
    }
    for (i = 0; i < program->rule_count; i++) {
        status = qjs_intl_segment_program_rule(program, i, &rules[i]);
        if (status)
            goto done;
        rules[i].left = mapping[rules[i].left];
        rules[i].right = mapping[rules[i].right] - left_count;
    }
    /* At boundary p, right states have consumed exactly the suffix [p,n)
       backwards. Record acceptance before consuming the preceding scalar. */
    if (right_count) {
        position = text->count;
        for (;;) {
            uint8_t *mask = right_masks + position * stride;
            for (i = 0; i < right_count; i++) {
                MachineRun *machine = &machines[left_count + i];
                mask[i / 8] |= accepts(machine) << (i & 7);
                if (position)
                    step(machine, result.classes[position - 1]);
            }
            if (!position)
                break;
            position--;
        }
    }
    for (position = 0; position <= text->count; position++) {
        result.breaks[position] = 1;
        result.rules[position] = QJS_INTL_SEGMENT_DEFAULT_RULE;
        if (position && position < text->count) {
            const uint8_t *mask = right_count ? right_masks + position * stride : NULL;
            for (i = 0; i < program->rule_count; i++) {
                const QJSIntlSegmentRule *rule = &rules[i];
                if (accepts(&machines[rule->left]) &&
                    (mask[rule->right / 8] & (1u << (rule->right & 7)))) {
                    result.breaks[position] = rule->breaks;
                    result.rules[position] = i;
                    break;
                }
            }
        }
        if (position < text->count) {
            for (i = 0; i < left_count; i++)
                step(&machines[i], result.classes[position]);
        }
    }
    status = QJS_INTL_OK;
done:
    if (scratch)
        allocator->free(allocator->opaque, scratch);
    if (status)
        qjs_intl_segment_scan_clear(&result);
    else
        *out = result;
    return status;
}
