/* Streaming default extended grapheme clusters, UAX #29 revision 49.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "grapheme.h"
#include "break-properties.h"
#include "utf16.h"

typedef struct GraphemeState {
    unsigned int previous, indic_linker, pictographic, odd_ri;
} GraphemeState;

static unsigned int gcb(uint16_t properties)
{
    return properties & UNICODE_BREAK_GCB_MASK;
}

static unsigned int incb(uint16_t properties)
{
    return (properties & UNICODE_BREAK_INCB_MASK) >> UNICODE_BREAK_INCB_SHIFT;
}

static int control(unsigned int property)
{
    return property == UNICODE_GCB_Control || property == UNICODE_GCB_CR ||
           property == UNICODE_GCB_LF;
}

static void advance(GraphemeState *state, uint16_t properties)
{
    unsigned int current = gcb(properties), indic = incb(properties);
    /* GB9c in Unicode 18 needs a linker followed only by InCB Extend. It no
       longer requires the consonant prefix used by the Unicode 17 rule. */
    if (indic == UNICODE_INCB_Linker)
        state->indic_linker = 1;
    else if (indic != UNICODE_INCB_Extend)
        state->indic_linker = 0;

    /* GB11: 1 = EP Extend*, 2 = EP Extend* ZWJ. An intervening character
       outside that expression clears it, even when GB9 joins the cluster. */
    if (properties & UNICODE_BREAK_EP)
        state->pictographic = 1;
    else if (current == UNICODE_GCB_ZWJ && state->pictographic == 1)
        state->pictographic = 2;
    else if (current != UNICODE_GCB_Extend || state->pictographic != 1)
        state->pictographic = 0;
    state->odd_ri = current == UNICODE_GCB_Regional_Indicator ? !state->odd_ri : 0;
    state->previous = current;
}

static int joins(const GraphemeState *state, uint16_t properties)
{
    unsigned int a = state->previous, b = gcb(properties);
    if (a == UNICODE_GCB_CR && b == UNICODE_GCB_LF) /* GB3 */
        return 1;
    if (control(a) || control(b)) /* GB4 / GB5 precede all extending rules. */
        return 0;
    if (a == UNICODE_GCB_L && (b == UNICODE_GCB_L || b == UNICODE_GCB_V ||
                              b == UNICODE_GCB_LV || b == UNICODE_GCB_LVT))
        return 1; /* GB6 */
    if ((a == UNICODE_GCB_LV || a == UNICODE_GCB_V) &&
        (b == UNICODE_GCB_V || b == UNICODE_GCB_T))
        return 1; /* GB7 */
    if ((a == UNICODE_GCB_LVT || a == UNICODE_GCB_T) && b == UNICODE_GCB_T)
        return 1; /* GB8 */
    if (b == UNICODE_GCB_Extend || b == UNICODE_GCB_ZWJ ||
        b == UNICODE_GCB_SpacingMark || a == UNICODE_GCB_Prepend)
        return 1; /* GB9 / GB9a / GB9b */
    if (state->indic_linker && incb(properties) == UNICODE_INCB_Consonant)
        return 1; /* GB9c */
    if (state->pictographic == 2 && (properties & UNICODE_BREAK_EP))
        return 1; /* GB11 */
    if (a == UNICODE_GCB_Regional_Indicator && b == UNICODE_GCB_Regional_Indicator &&
        state->odd_ri)
        return 1; /* GB12 / GB13 */
    return 0; /* GB999 */
}

size_t unicode_grapheme_next(const uint16_t *text, size_t length, size_t start)
{
    GraphemeState state = {0};
    size_t position = start;
    uint16_t properties;
    if (start >= length)
        return length;
    properties = unicode_break_properties(unicode_utf16_next(text, length, &position));
    advance(&state, properties);
    while (position < length) {
        size_t next = position;
        properties = unicode_break_properties(unicode_utf16_next(text, length, &next));
        if (!joins(&state, properties))
            return position;
        advance(&state, properties);
        position = next;
    }
    return length; /* GB2; zero/start is the caller's GB1 boundary. */
}
