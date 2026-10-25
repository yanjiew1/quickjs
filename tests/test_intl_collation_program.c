/* Program isolation, semantic failures and complete cleanup under injected OOM.
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
#include "../tools/intl/collation-program.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t intl_root_fixture[];
extern const size_t intl_root_fixture_size;
#define FIXTURE(name) \
    extern const uint8_t intl_program_##name[]; \
    extern const size_t intl_program_##name##_size
FIXTURE(success);
FIXTURE(before);
FIXTURE(compatible);
FIXTURE(inert);
FIXTURE(combining);
FIXTURE(hangul);
FIXTURE(interior);
FIXTURE(prefix);
FIXTURE(reset);
FIXTURE(unknown_reset);
FIXTURE(setting);
FIXTURE(unsupported);
FIXTURE(no_reset);
FIXTURE(dangling);
FIXTURE(double_reset);
FIXTURE(before_mismatch);
FIXTURE(empty);
#undef FIXTURE

typedef struct State {
    size_t calls, fail_at, live;
    unsigned int ends, callback_failure;
} State;

static void *allocate(void *opaque, size_t size)
{
    State *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    assert(ptr);
    state->live++;
    return ptr;
}

static void *resize(void *opaque, void *ptr, size_t size)
{
    State *state = opaque;
    void *next;
    assert(ptr && size);
    if (++state->calls == state->fail_at)
        return NULL;
    next = realloc(ptr, size);
    assert(next);
    return next;
}

static void release(void *opaque, void *ptr)
{
    State *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static QJSIntlAllocator allocator(State *state)
{
    return (QJSIntlAllocator){state, allocate, resize, release};
}

static QJSIntlRelationElement one(QJSIntlCollationBuilder *builder, uint32_t code)
{
    QJSIntlRelationMapping mapping;
    QJSIntlRelationElement ce;
    assert(!qjs_intl_collation_builder_lookup(builder, &code, 1, &mapping));
    assert(mapping.count == 1);
    ce = mapping.elements[0];
    qjs_intl_relation_mapping_clear(&builder->allocator, &mapping);
    return ce;
}

static int compare(QJSIntlCollationBuilder *builder, unsigned int axis, uint32_t a, uint32_t b)
{
    int order;
    assert(!qjs_intl_weight_compare(builder->relations.axes + axis, a, b, &order));
    return order;
}

static QJSIntlStatus finished(void *opaque, const QJSIntlIRProgram *identity, QJSIntlCollationProgram *program)
{
    State *state = opaque;
    QJSIntlCollationBuilder *builder = &program->builder;
    uint32_t i;
    state->ends++;
    if (state->callback_failure)
        return QJS_INTL_UNSUPPORTED;
    if (!memcmp(identity->locale.bytes, "en", 2)) {
        QJSIntlRelationElement g = one(builder, 'g'), h = one(builder, 'h'), last = one(builder, 'i');
        QJSIntlRelationMapping j, k, original, contextual;
        const uint32_t ax[] = {'a', 'x'}, az[] = {'a', 'z'};
        uint32_t hani, latn;
        assert(program->policy.strength == 2 && program->policy.case_first == QJS_INTL_COLLATION_UPPER_FIRST);
        assert(compare(builder, 0, g.weights[0], h.weights[0]) < 0);
        assert(compare(builder, 0, h.weights[0], last.weights[0]) < 0);
        assert(!qjs_intl_collation_builder_lookup(builder, (const uint32_t[]){'j'}, 1, &j));
        assert(!qjs_intl_collation_builder_lookup(builder, (const uint32_t[]){'k'}, 1, &k));
        assert(j.count == 2 && k.count == j.count);
        for (i = 0; i < j.count; i++) {
            unsigned int axis;
            for (axis = 0; axis < 4; axis++)
                assert(j.elements[i].weights[axis] == k.elements[i].weights[axis]);
            assert(j.elements[i].root_primary == k.elements[i].root_primary);
            assert(j.elements[i].case_value == k.elements[i].case_value);
        }
        qjs_intl_relation_mapping_clear(&builder->allocator, &j);
        qjs_intl_relation_mapping_clear(&builder->allocator, &k);
        /* Suppression removed the old contextual a|x mapping. The later a|z
           relation reintroduced a context without undoing root suppression. */
        assert(!qjs_intl_collation_builder_lookup(builder, ax, 2, &original));
        assert(!qjs_intl_collation_builder_lookup(builder, az, 2, &contextual));
        assert(original.count == 2 && contextual.count == 2);
        assert(compare(builder, 0, original.elements[1].weights[0], contextual.elements[1].weights[0]) != 0);
        qjs_intl_relation_mapping_clear(&builder->allocator, &original);
        qjs_intl_relation_mapping_clear(&builder->allocator, &contextual);
        assert(!qjs_intl_collation_reorder_rank(&program->reorder, 6, &hani));
        assert(!qjs_intl_collation_reorder_rank(&program->reorder, 5, &latn));
        assert(hani < latn);
    } else {
        assert(identity->locale.length == 2 && !memcmp(identity->locale.bytes, "fr", 2));
        assert(program->policy.strength == 3 && program->policy.case_first == QJS_INTL_COLLATION_CASE_OFF);
        assert(program->policy.numeric && builder->map.live_count == 1);
        for (i = 0; i < program->reorder.count; i++) {
            uint32_t rank;
            assert(!qjs_intl_collation_reorder_rank(&program->reorder, i, &rank) && rank == i);
        }
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus count_end(void *opaque, const QJSIntlIRProgram *identity, QJSIntlCollationProgram *program)
{
    State *state = opaque;
    (void)identity;
    (void)program;
    state->ends++;
    return QJS_INTL_OK;
}

static QJSIntlStatus execute(const QJSIntlRootInput *root, const uint8_t *bytes, size_t size,
                              State *state, QJSIntlCollationProgramFinalizer callback, QJSIntlIRDiagnostic *error)
{
    QJSIntlIRView view;
    QJSIntlAllocator a = allocator(state);
    assert(!qjs_intl_ir_open(bytes, size, NULL, &view, NULL));
    return qjs_intl_collation_program_execute(root, &view, &a, callback, state, error);
}

static void test_success_and_callback_failure(const QJSIntlRootInput *root)
{
    State state = {0};
    QJSIntlIRDiagnostic error;
    assert(!execute(root, intl_program_success, intl_program_success_size, &state, finished, &error));
    assert(state.ends == 2 && !state.live);
    state = (State){.callback_failure = 1};
    assert(execute(root, intl_program_success, intl_program_success_size, &state, finished, &error) == QJS_INTL_UNSUPPORTED);
    assert(state.ends == 1 && !state.live && !error.source.length && !error.program_index);
    state = (State){0};
    assert(!execute(root, intl_program_before, intl_program_before_size, &state, NULL, &error));
    assert(!state.live);
    assert(!execute(root, intl_program_compatible, intl_program_compatible_size, &state, NULL, &error));
    assert(!state.live);
    assert(!execute(root, intl_program_empty, intl_program_empty_size, &state, count_end, &error));
    assert(state.ends == 1 && !state.live);
}

static void test_semantic_failures(const QJSIntlRootInput *root)
{
    struct Fixture { const uint8_t *bytes; size_t size; QJSIntlStatus status; } fixtures[] = {
#define BAD(name, status) {intl_program_##name, intl_program_##name##_size, status}
        BAD(inert, QJS_INTL_DATA_ERROR), BAD(interior, QJS_INTL_DATA_ERROR),
        BAD(combining, QJS_INTL_DATA_ERROR), BAD(hangul, QJS_INTL_DATA_ERROR),
        BAD(prefix, QJS_INTL_DATA_ERROR), BAD(reset, QJS_INTL_UNSUPPORTED),
        BAD(unknown_reset, QJS_INTL_DATA_ERROR),
        BAD(setting, QJS_INTL_DATA_ERROR), BAD(unsupported, QJS_INTL_UNSUPPORTED),
        BAD(no_reset, QJS_INTL_DATA_ERROR), BAD(dangling, QJS_INTL_DATA_ERROR),
        BAD(double_reset, QJS_INTL_DATA_ERROR), BAD(before_mismatch, QJS_INTL_DATA_ERROR)
#undef BAD
    };
    unsigned int i;
    for (i = 0; i < sizeof(fixtures) / sizeof(*fixtures); i++) {
        State state = {0};
        QJSIntlIRDiagnostic error;
        assert(execute(root, fixtures[i].bytes, fixtures[i].size, &state, count_end, &error) == fixtures[i].status);
        assert(!state.ends && !state.live && !error.program_index);
        assert(error.source.length == 1 && error.source.bytes[0] == 'x' && error.line == 1);
        if (fixtures[i].bytes == intl_program_dangling)
            assert(error.instruction_index == 0 && error.column == 1);
    }
}

static void test_pairing_and_oom(const QJSIntlRootInput *root)
{
    State state = {0};
    size_t count, i;
    QJSIntlIRDiagnostic error;
    QJSIntlIRView view;
    QJSIntlAllocator a = allocator(&state);
    uint8_t *copy = malloc(intl_program_success_size + 1);
    assert(copy);
    memcpy(copy + 1, intl_program_success, intl_program_success_size);
    copy[52] ^= 1; /* Check the final identity byte before any allocation. */
    assert(!qjs_intl_ir_open(copy + 1, intl_program_success_size, NULL, &view, NULL));
    assert(qjs_intl_collation_program_execute(root, &view, &a, count_end, &state, &error) == QJS_INTL_DATA_ERROR);
    assert(!state.calls && !state.ends && !state.live);
    copy[52] ^= 1;
    assert(!qjs_intl_ir_open(copy + 1, intl_program_success_size, NULL, &view, NULL));
    assert(!qjs_intl_collation_program_execute(root, &view, &a, count_end, &state, NULL));
    count = state.calls;
    assert(count && state.ends == 2 && !state.live);
    for (i = 1; i <= count; i++) {
        state = (State){.fail_at = i};
        assert(qjs_intl_collation_program_execute(root, &view, &a, count_end, &state, &error) == QJS_INTL_NO_MEMORY);
        assert(!state.live && state.ends < 2);
    }
    free(copy);
}

int main(void)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRootInput root;
    assert(!qjs_intl_root_input_open(intl_root_fixture, intl_root_fixture_size, &a, &root));
    assert(!state.live);
    test_success_and_callback_failure(&root);
    test_semantic_failures(&root);
    test_pairing_and_oom(&root);
    return 0;
}
