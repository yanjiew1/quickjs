/* Allocation-free Unicode properties for text boundary algorithms.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef UNICODE_BREAK_PROPERTIES_H
#define UNICODE_BREAK_PROPERTIES_H
#include <stdint.h>

#define UNICODE_GRAPHEME_BREAK
typedef enum {
#define DEF(name) UNICODE_GCB_ ## name,
#include "break-properties-def.h"
#undef DEF
    UNICODE_GCB_COUNT
} UnicodeGraphemeBreak;
#undef UNICODE_GRAPHEME_BREAK

#define UNICODE_WORD_BREAK
typedef enum {
#define DEF(name) UNICODE_WB_ ## name,
#include "break-properties-def.h"
#undef DEF
    UNICODE_WB_COUNT
} UnicodeWordBreak;
#undef UNICODE_WORD_BREAK

#define UNICODE_SENTENCE_BREAK
typedef enum {
#define DEF(name) UNICODE_SB_ ## name,
#include "break-properties-def.h"
#undef DEF
    UNICODE_SB_COUNT
} UnicodeSentenceBreak;
#undef UNICODE_SENTENCE_BREAK

#define UNICODE_INDIC_CONJUNCT_BREAK
typedef enum {
#define DEF(name) UNICODE_INCB_ ## name,
#include "break-properties-def.h"
#undef DEF
    UNICODE_INCB_COUNT
} UnicodeIndicConjunctBreak;
#undef UNICODE_INDIC_CONJUNCT_BREAK

/* All five properties share one lookup. These private field positions are
   also used by the table generator; zero means Other/None/not pictographic.
   They do not extend ECMAScript's accepted RegExp property-name surface. */
enum {
    UNICODE_BREAK_GCB_SHIFT = 0, UNICODE_BREAK_GCB_MASK = 15,
    UNICODE_BREAK_INCB_SHIFT = 4, UNICODE_BREAK_INCB_MASK = 3 << 4,
    UNICODE_BREAK_EP = 1 << 6,
    UNICODE_BREAK_WB_SHIFT = 7, UNICODE_BREAK_WB_MASK = 31 << 7,
    UNICODE_BREAK_SB_SHIFT = 12, UNICODE_BREAK_SB_MASK = 15 << 12,
    UNICODE_BREAK_BUCKET_SHIFT = 10
};

/* Accept Unicode code points, including unpaired surrogate code units.
   Out-of-range values return the default property word. No allocation. */
uint16_t unicode_break_properties(uint32_t code);
#endif
