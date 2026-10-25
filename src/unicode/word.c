/* Default word boundaries, Unicode 18 / UAX #29 revision 49.
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
#include "word.h"
#include "break-properties.h"
#include "utf16.h"

typedef struct WordState {
    unsigned int previous, significant, before, odd_ri;
} WordState;

static unsigned int wb(uint16_t properties)
{
    return (properties & UNICODE_BREAK_WB_MASK) >> UNICODE_BREAK_WB_SHIFT;
}

static int ignored(unsigned int property)
{
    return property == UNICODE_WB_Extend || property == UNICODE_WB_Format ||
           property == UNICODE_WB_ZWJ;
}

static int newline(unsigned int property)
{
    return property == UNICODE_WB_CR || property == UNICODE_WB_LF ||
           property == UNICODE_WB_Newline;
}

static int letter(unsigned int property)
{
    return property == UNICODE_WB_ALetter || property == UNICODE_WB_Hebrew_Letter;
}

static int mid_letter(unsigned int property)
{
    return property == UNICODE_WB_MidLetter || property == UNICODE_WB_MidNumLet ||
           property == UNICODE_WB_Single_Quote;
}

static int mid_numeric(unsigned int property)
{
    return property == UNICODE_WB_MidNum || property == UNICODE_WB_MidNumLet ||
           property == UNICODE_WB_Single_Quote;
}

static int extender(unsigned int property)
{
    return letter(property) || property == UNICODE_WB_Numeric ||
           property == UNICODE_WB_Katakana || property == UNICODE_WB_ExtendNumLet;
}

static void advance(WordState *state, unsigned int property)
{
    state->previous = property;
    if (!ignored(property)) {
        state->before = state->significant;
        state->significant = property;
        state->odd_ri = property == UNICODE_WB_Regional_Indicator ? !state->odd_ri : 0;
    }
}

/* Only punctuation rules require this lookahead. Each skipped run is visited
   at most once here and once in the forward scan, even if punctuation breaks. */
static unsigned int following(const uint16_t *text, size_t length, size_t position)
{
    while (position < length) {
        unsigned int property = wb(unicode_break_properties(unicode_utf16_next(text, length, &position)));
        if (!ignored(property))
            return property;
    }
    return UNICODE_WB_Other;
}

static int joins(const WordState *state, uint16_t properties,
                  const uint16_t *text, size_t length, size_t next)
{
    unsigned int raw = state->previous, a = state->significant, b = wb(properties);
    if (raw == UNICODE_WB_CR && b == UNICODE_WB_LF)
        return 1; /* WB3 */
    if (newline(raw) || newline(b))
        return 0; /* WB3a / WB3b */
    if (raw == UNICODE_WB_ZWJ && (properties & UNICODE_BREAK_EP))
        return 1; /* WB3c: adjacent ZWJ, before the ignore rule. */
    if (raw == UNICODE_WB_WSegSpace && b == UNICODE_WB_WSegSpace)
        return 1; /* WB3d does not cross ignored characters. */
    if (ignored(b))
        return 1; /* WB4 */
    if (letter(a) && letter(b))
        return 1; /* WB5 */
    if (letter(a) && mid_letter(b) && letter(following(text, length, next)))
        return 1; /* WB6 */
    if (letter(state->before) && mid_letter(a) && letter(b))
        return 1; /* WB7 */
    if (a == UNICODE_WB_Hebrew_Letter && b == UNICODE_WB_Single_Quote)
        return 1; /* WB7a */
    if (a == UNICODE_WB_Hebrew_Letter && b == UNICODE_WB_Double_Quote &&
        following(text, length, next) == UNICODE_WB_Hebrew_Letter)
        return 1; /* WB7b */
    if (state->before == UNICODE_WB_Hebrew_Letter && a == UNICODE_WB_Double_Quote &&
        b == UNICODE_WB_Hebrew_Letter)
        return 1; /* WB7c */
    if ((a == UNICODE_WB_Numeric && (b == UNICODE_WB_Numeric || letter(b))) ||
        (letter(a) && b == UNICODE_WB_Numeric))
        return 1; /* WB8 / WB9 / WB10 */
    if (state->before == UNICODE_WB_Numeric && mid_numeric(a) && b == UNICODE_WB_Numeric)
        return 1; /* WB11 */
    if (a == UNICODE_WB_Numeric && mid_numeric(b) &&
        following(text, length, next) == UNICODE_WB_Numeric)
        return 1; /* WB12 */
    if (a == UNICODE_WB_Katakana && b == UNICODE_WB_Katakana)
        return 1; /* WB13 */
    if ((extender(a) && b == UNICODE_WB_ExtendNumLet) ||
        (a == UNICODE_WB_ExtendNumLet && extender(b)))
        return 1; /* WB13a / WB13b */
    if (a == UNICODE_WB_Regional_Indicator && b == UNICODE_WB_Regional_Indicator && state->odd_ri)
        return 1; /* WB15 / WB16 */
    return 0; /* WB999 */
}

size_t unicode_word_next(const uint16_t *text, size_t length, size_t start)
{
    WordState state = {UNICODE_WB_Other, UNICODE_WB_Other, UNICODE_WB_Other, 0};
    size_t position = start;
    uint16_t properties;
    if (start >= length)
        return length;
    properties = unicode_break_properties(unicode_utf16_next(text, length, &position));
    advance(&state, wb(properties));
    while (position < length) {
        size_t next = position;
        properties = unicode_break_properties(unicode_utf16_next(text, length, &next));
        if (!joins(&state, properties, text, length, next))
            return position;
        advance(&state, wb(properties));
        position = next;
    }
    return length;
}
