/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NATIVE_PACK_H
#define QJS_INTL_NATIVE_PACK_H

#include "../common/types.h"

enum {
    QJS_INTL_SECTION_STRINGS = 1,
    QJS_INTL_SECTION_LOCALES = 2,
    QJS_INTL_SECTION_ALIASES = 3,
    QJS_INTL_SECTION_LIKELY = 4,
    QJS_INTL_SECTION_KEYS = 5,
    QJS_INTL_SECTION_TYPES = 6,
    QJS_INTL_SECTION_WEEKS = 7,
    QJS_INTL_SECTION_CALENDAR_PREFERENCES = 8,
    QJS_INTL_SECTION_HOUR_PREFERENCES = 9,
    QJS_INTL_SECTION_LISTS = 10,
    QJS_INTL_SECTION_SCRIPT_DIRECTIONS = 11,
    QJS_INTL_SECTION_LIST_CONTEXTS = 12,
    QJS_INTL_SECTION_NUMBERING_SYSTEMS = 13,
    QJS_INTL_SECTION_NUMBERING_PREFERENCES = 14,
    QJS_INTL_SECTION_NUMBER_SYMBOLS = 15,
    QJS_INTL_SECTION_PLURALS = 16,
    QJS_INTL_SECTION_NUMBER_PATTERNS = 17,
    QJS_INTL_SECTION_COMPACT_PATTERNS = 18,
    QJS_INTL_SECTION_CURRENCIES = 19,
    QJS_INTL_SECTION_NUMBER_LAYOUTS = 20,
    QJS_INTL_SECTION_UNIT_PATTERNS = 21,
    QJS_INTL_SECTION_UNIT_GRAMMAR = 22,
    QJS_INTL_SECTION_RELATIVE_PATTERNS = 23,
    QJS_INTL_SECTION_DURATION_PATTERNS = 24,
    QJS_INTL_SECTION_DATE_FIELD_NAMES = 25,
    QJS_INTL_SECTION_DISPLAY_NAMES = 26,
    QJS_INTL_SECTION_DATE_STYLES = 27,
    QJS_INTL_SECTION_DATE_COMBINATIONS = 28,
    QJS_INTL_SECTION_CALENDAR_NAMES = 29,
    QJS_INTL_SECTION_DATE_AVAILABLE_FORMATS = 30,
    QJS_INTL_SECTION_DATE_INTERVALS = 31,
    QJS_INTL_SECTION_TIMEZONE_NAMES = 32,
    QJS_INTL_SECTION_TIMEZONE_PRESENTATION = 33,
};

typedef struct QJSIntlPack {
    const uint8_t *bytes;
    size_t length;
    const uint8_t *generation; /* 32-byte identity, not an authenticity check. */
    uint32_t section_count;
    unsigned int profile; /* 0: minimal, 1: native. */
} QJSIntlPack;

typedef struct QJSIntlSection {
    const uint8_t *bytes;
    size_t length;
    uint32_t count;
} QJSIntlSection;

/* Borrow the immutable blob for the lifetime of the view and its sections.
   Outputs must not alias inputs or the blob; non-NULL outputs are zeroed on
   failure. Accessors require an unchanged view from successful open. Open
   checks the section envelope only; each owner validates its record schema
   before use. Unknown sections do not establish any service capability. */
QJSIntlStatus qjs_intl_pack_open(const void *bytes, size_t length,
                               QJSIntlPack *out);
QJSIntlStatus qjs_intl_pack_section(const QJSIntlPack *pack, uint32_t identity,
                                  QJSIntlSection *out);

#endif
