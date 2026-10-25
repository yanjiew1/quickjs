/* Full structural preflight, borrowed descriptors and exact callback errors.
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
#include "../tools/intl/collation-ir.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t intl_ir_fixture[];
extern const size_t intl_ir_fixture_size;

typedef struct Visit {
    unsigned int calls, begins, nodes, ends, fail_at, nul, supplementary;
    QJSIntlStatus failure;
} Visit;

static QJSIntlStatus next(Visit *state)
{
    return ++state->calls == state->fail_at ? state->failure : QJS_INTL_OK;
}

static QJSIntlStatus begin(void *opaque, const QJSIntlIRProgram *program)
{
    Visit *state = opaque;
    assert(program->locale.length == 2 && program->collation.length == 8 && !program->alternate.length);
    assert(!memcmp(program->collation.bytes, "standard", 8));
    state->begins++;
    return next(state);
}

static QJSIntlStatus instruction(void *opaque, const QJSIntlIRInstruction *node)
{
    Visit *state = opaque;
    uint32_t i, j;
    assert(node->source.length == 1 && node->source.bytes[0] == 'x' && node->line && node->column);
    assert(node->kind >= QJS_INTL_IR_RESET && node->kind <= QJS_INTL_IR_OPTIMIZE);
    for (i = 0; i < 3; i++) {
        for (j = 0; j < node->fields[i].count; j++) {
            uint32_t code = qjs_intl_ir_scalar(node->fields + i, j);
            state->nul |= !code;
            state->supplementary |= code == 0x1f600;
        }
    }
    state->nodes++;
    return next(state);
}

static QJSIntlStatus end(void *opaque, const QJSIntlIRProgram *program)
{
    Visit *state = opaque;
    assert(program->instruction_count == 0 || program->instruction_count == 6);
    state->ends++;
    return next(state);
}

static const QJSIntlIRVisitor visitor = {begin, instruction, end};

static void put32(uint8_t *p, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        p[i] = value >> (i * 8);
}

static void test_unaligned_and_errors(void)
{
    QJSIntlIRView view;
    QJSIntlIRDiagnostic error;
    Visit state = {0};
    uint8_t expected[32], *bytes = malloc(intl_ir_fixture_size + 1);
    unsigned int i;
    assert(bytes);
    memcpy(bytes + 1, intl_ir_fixture, intl_ir_fixture_size);
    for (i = 0; i < 32; i++)
        expected[i] = i;
    assert(!qjs_intl_ir_open(bytes + 1, intl_ir_fixture_size, expected, &view, &error));
    assert(view.bytes == bytes + 1 && view.fingerprint == bytes + 21 && view.program_count == 2);
    assert(!qjs_intl_ir_visit(&view, &visitor, &state, &error));
    assert(state.calls == 10 && state.begins == 2 && state.nodes == 6 && state.ends == 2);
    assert(state.nul && state.supplementary);
    for (i = 1; i <= 10; i++) {
        state = (Visit){.fail_at = i, .failure = i % 2 ? QJS_INTL_NO_MEMORY : QJS_INTL_UNSUPPORTED};
        assert(qjs_intl_ir_visit(&view, &visitor, &state, &error) == state.failure);
        assert(state.calls == i); /* No success-finalizer or later callbacks. */
        if (i >= 2 && i <= 7)
            assert(error.source.length == 1 && error.source.bytes[0] == 'x' && error.line == 1);
        else
            assert(!error.source.length && !error.line);
    }
    expected[0] ^= 1;
    assert(qjs_intl_ir_open(bytes + 1, intl_ir_fixture_size, expected, &view, &error) == QJS_INTL_DATA_ERROR);
    assert(!view.bytes && !view.fingerprint && !view.length && !view.program_count);
    state = (Visit){0};
    assert(qjs_intl_ir_visit(&view, &visitor, &state, &error) == QJS_INTL_INVALID_ARGUMENT && !state.calls);
    assert(qjs_intl_ir_open(NULL, 0, NULL, &view, NULL) == QJS_INTL_INVALID_ARGUMENT);
    free(bytes);
}

static void test_late_corruption_prevents_visits(void)
{
    uint8_t *bytes = malloc(intl_ir_fixture_size + 1);
    QJSIntlIRView view;
    QJSIntlIRDiagnostic error;
    Visit state = {0};
    uint32_t first_size;
    assert(bytes);
    memcpy(bytes, intl_ir_fixture, intl_ir_fixture_size);
    first_size = qjs_intl_read_u32(bytes + 52);
    /* Only the final program is malformed; the earlier program remains valid. */
    put32(bytes + 52 + first_size, 19);
    assert(qjs_intl_ir_open(bytes, intl_ir_fixture_size, NULL, &view, &error) == QJS_INTL_DATA_ERROR);
    assert(!view.bytes && error.program_index == 1);
    assert(qjs_intl_ir_visit(&view, &visitor, &state, &error) == QJS_INTL_INVALID_ARGUMENT && !state.calls);
    memcpy(bytes, intl_ir_fixture, intl_ir_fixture_size);
    bytes[intl_ir_fixture_size] = 0;
    put32(bytes + 12, (uint32_t)intl_ir_fixture_size + 1);
    assert(qjs_intl_ir_open(bytes, intl_ir_fixture_size + 1, NULL, &view, &error) == QJS_INTL_DATA_ERROR);
    assert(!view.bytes);
    free(bytes);
}

static void test_empty_view_and_optional_callbacks(void)
{
    uint8_t bytes[52];
    QJSIntlIRView view;
    QJSIntlIRVisitor empty = {0};
    Visit state = {0};
    memcpy(bytes, intl_ir_fixture, sizeof(bytes));
    put32(bytes + 12, sizeof(bytes));
    put32(bytes + 16, 0);
    assert(!qjs_intl_ir_open(bytes, sizeof(bytes), NULL, &view, NULL));
    assert(!qjs_intl_ir_visit(&view, &visitor, &state, NULL) && !state.calls);
    assert(!qjs_intl_ir_visit(&view, &empty, NULL, NULL));
    assert(qjs_intl_ir_visit(&view, NULL, NULL, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_ir_open(bytes, SIZE_MAX, NULL, &view, NULL) == QJS_INTL_DATA_ERROR);
    assert(!view.bytes);
}

int main(void)
{
    test_unaligned_and_errors();
    test_late_corruption_prevents_visits();
    test_empty_view_and_optional_callbacks();
    return 0;
}
