/* Generated from Intl Era/MonthCode table-calendar-types.
 * Proposal 5833eae6c9079ffbb06a9ab7cc5128ebbf6e9ac7, 2026-03-19.
 * Reviewed 2026-10-08. Sixteen canonical types; two accepted aliases.
 * Regenerate with this packet's prepare-source.py; no ICU enumeration. */
#ifndef QUICKJS_INTL_CALENDAR_TYPES_H
#define QUICKJS_INTL_CALENDAR_TYPES_H
static const char *const intl_calendar_types[] = {
    "buddhist",
    "chinese",
    "coptic",
    "dangi",
    "ethioaa",
    "ethiopic",
    "gregory",
    "hebrew",
    "indian",
    "islamic-civil",
    "islamic-tbla",
    "islamic-umalqura",
    "iso8601",
    "japanese",
    "persian",
    "roc",
};
static const struct { const char *alias, *canonical; }
intl_calendar_aliases[] = {
    { "ethiopic-amete-alem", "ethioaa" },
    { "islamicc", "islamic-civil" },
};
#endif
