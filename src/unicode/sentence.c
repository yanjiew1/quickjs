/* Default sentence boundaries, Unicode 18 / UAX #29 revision 49.
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
#include "sentence.h"
#include "break-properties.h"
#include "utf16.h"

typedef struct SentenceState {
    unsigned int previous, significant, before;
    unsigned int terminator, spaces, probe_done, lower_ahead;
} SentenceState;

static unsigned int sb(uint16_t properties)
{
    return (properties & UNICODE_BREAK_SB_MASK) >> UNICODE_BREAK_SB_SHIFT;
}

static int paragraph(unsigned int property)
{
    return property == UNICODE_SB_CR || property == UNICODE_SB_LF || property == UNICODE_SB_Sep;
}

static int ignored(unsigned int property)
{
    return property == UNICODE_SB_Extend || property == UNICODE_SB_Format;
}

static int terminal(unsigned int property)
{
    return property == UNICODE_SB_ATerm || property == UNICODE_SB_STerm;
}

static void advance(SentenceState *state, unsigned int property)
{
    state->previous = property;
    if (ignored(property))
        return;
    if (terminal(property)) {
        state->terminator = property;
        state->spaces = state->probe_done = state->lower_ahead = 0;
    } else if (state->terminator) {
        if (property == UNICODE_SB_Sp)
            state->spaces = 1;
        else if (property != UNICODE_SB_Close || state->spaces)
            state->terminator = 0;
    }
    state->before = state->significant;
    state->significant = property;
}

static int following_lower(const uint16_t *text, size_t length, size_t position)
{
    while (position < length) {
        unsigned int property = sb(unicode_break_properties(unicode_utf16_next(text, length, &position)));
        if (property == UNICODE_SB_Lower)
            return 1;
        if (property == UNICODE_SB_OLetter || property == UNICODE_SB_Upper ||
            paragraph(property) || terminal(property))
            return 0;
    }
    return 0;
}

static int joins(SentenceState *state, unsigned int property,
                  const uint16_t *text, size_t length, size_t position)
{
    if (state->previous == UNICODE_SB_CR && property == UNICODE_SB_LF)
        return 1; /* SB3 */
    if (paragraph(state->previous))
        return 0; /* SB4 precedes ignoring. */
    if (ignored(property))
        return 1; /* SB5 */
    if (state->significant == UNICODE_SB_ATerm && property == UNICODE_SB_Numeric)
        return 1; /* SB6 */
    if ((state->before == UNICODE_SB_Upper || state->before == UNICODE_SB_Lower) &&
        state->significant == UNICODE_SB_ATerm && property == UNICODE_SB_Upper)
        return 1; /* SB7 */
    if (state->terminator == UNICODE_SB_ATerm) {
        /* Close/Sp runs can be arbitrarily long. Probe once per terminal
           prefix, not again at every candidate boundary along the run. A
           probe stops at the next terminal/letter/paragraph, so complete
           forward iteration still performs only bounded extra scanning. */
        if (!state->probe_done) {
            state->lower_ahead = following_lower(text, length, position);
            state->probe_done = 1;
        }
        if (state->lower_ahead)
            return 1; /* SB8 */
    }
    if (state->terminator) {
        if (property == UNICODE_SB_SContinue || terminal(property))
            return 1; /* SB8a */
        if ((!state->spaces && property == UNICODE_SB_Close) ||
            property == UNICODE_SB_Sp || paragraph(property))
            return 1; /* SB9 / SB10 */
        return 0; /* SB11; paragraph boundaries are already handled by SB4. */
    }
    return 1; /* SB998 */
}

size_t unicode_sentence_next(const uint16_t *text, size_t length, size_t start)
{
    SentenceState state = {0};
    size_t position = start;
    unsigned int property;
    if (start >= length)
        return length;
    property = sb(unicode_break_properties(unicode_utf16_next(text, length, &position)));
    advance(&state, property);
    while (position < length) {
        size_t next = position;
        property = sb(unicode_break_properties(unicode_utf16_next(text, length, &next)));
        if (!joins(&state, property, text, length, position))
            return position;
        advance(&state, property);
        position = next;
    }
    return length;
}
