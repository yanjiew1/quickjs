/* ECMA402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "matching.h"
#include "identifier.h"
#include <string.h>

/* Removing the Unicode extension can join two nonadjacent input slices.
   Retain that view instead of allocating or truncating away later extensions. */
typedef struct Candidate {
    QJSIntlBytes head, tail;
} Candidate;

static unsigned char at(const Candidate *candidate, size_t index)
{
    return index < candidate->head.length
               ? candidate->head.data[index]
               : candidate->tail.data[index - candidate->head.length];
}

static int compare(const Candidate *candidate, size_t length, QJSIntlBytes name)
{
    size_t i, common = length < name.length ? length : name.length;
    for (i = 0; i < common; i++) {
        unsigned char a = at(candidate, i), b = name.data[i];
        if (a != b)
            return a < b ? -1 : 1;
    }
    return (length > name.length) - (length < name.length);
}

QJSIntlStatus qjs_intl_lookup_matching_locale(const QJSIntlBytes *available,
                                              size_t available_count,
                                              const QJSIntlBytes *requested,
                                              size_t requested_count,
                                              QJSIntlLocaleMatch *out)
{
    size_t request;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!available && available_count) || (!requested && requested_count))
        return QJS_INTL_INVALID_ARGUMENT;
    for (request = 0; request < requested_count; request++) {
        QJSIntlLocaleComponents parts;
        QJSIntlStatus status = qjs_intl_locale_components(requested[request], &parts);
        Candidate candidate = {requested[request], {NULL, 0}};
        size_t length;

        if (status)
            return status;
        if (parts.unicode_extension.data) {
            size_t end;
            candidate.head.length = parts.unicode_extension.data - candidate.head.data;
            end = candidate.head.length + parts.unicode_extension.length;
            candidate.tail = (QJSIntlBytes){requested[request].data + end,
                                            requested[request].length - end};
        }
        length = candidate.head.length + candidate.tail.length;
        while (length) {
            size_t low = 0, high = available_count, pos = length;
            while (low < high) {
                size_t middle = low + (high - low) / 2;
                int order = compare(&candidate, length, available[middle]);
                if (order < 0)
                    high = middle;
                else if (order > 0)
                    low = middle + 1;
                else {
                    out->found = 1;
                    out->available_index = middle;
                    out->requested_index = request;
                    out->extension = parts.unicode_extension;
                    return QJS_INTL_OK;
                }
            }
            while (pos && at(&candidate, pos - 1) != '-')
                pos--;
            if (!pos)
                break;
            pos--;
            while (pos >= 2 && at(&candidate, pos - 2) == '-')
                pos -= 2;
            length = pos;
        }
    }
    return QJS_INTL_OK;
}
