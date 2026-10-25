/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "preference-data.h"
#include "../native/wire.h"
#include <stdlib.h>
#include <string.h>

static uint32_t field(const QJSIntlSection *table, unsigned int width, uint32_t row,
                      unsigned int column)
{
    return qjs_intl_read_u32(table->bytes + ((size_t)row * width + column) * 4);
}

static int lower(int c)
{
    return c >= 'a' && c <= 'z';
}
static int upper(int c)
{
    return c >= 'A' && c <= 'Z';
}
static int digit(int c)
{
    return c >= '0' && c <= '9';
}

static int region(QJSIntlBytes key)
{
    return (key.length == 2 && upper(key.data[0]) && upper(key.data[1])) ||
           (key.length == 3 && digit(key.data[0]) && digit(key.data[1]) &&
            digit(key.data[2]));
}

static int target(QJSIntlBytes key)
{
    size_t i;
    if (region(key))
        return 1;
    for (i = 0; i < key.length && lower(key.data[i]); i++)
        ;
    return i >= 2 && i <= 8 && i < key.length && key.data[i] == '-' &&
           region((QJSIntlBytes){key.data + i + 1, key.length - i - 1});
}

static int script(QJSIntlBytes key)
{
    return key.length == 4 && upper(key.data[0]) && lower(key.data[1]) &&
           lower(key.data[2]) && lower(key.data[3]);
}

static int compare(const void *left, const void *right)
{
    const QJSIntlBytes *a = left, *b = right;
    size_t length = a->length < b->length ? a->length : b->length;
    int order = length ? memcmp(a->data, b->data, length) : 0;
    return order ? order : (a->length > b->length) - (a->length < b->length);
}

static int calendar(QJSIntlBytes token)
{
    size_t i;
    if (!token.length || !lower(token.data[0]))
        return 0;
    for (i = 1; i < token.length; i++) {
        int c = token.data[i];
        if (c == '-') {
            if (i + 1 == token.length || token.data[i - 1] == '-')
                return 0;
        } else if (!lower(c) && !digit(c)) {
            return 0;
        }
    }
    return 1;
}

static QJSIntlStatus tokens(const QJSIntlAllocator *allocator, QJSIntlBytes text,
                            uint32_t count, int hours)
{
    QJSIntlBytes *items;
    size_t i, start = 0, index = 0;
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;

    if (!count || count > text.length || (hours && count > 4))
        return status;
    if (sizeof(*items) > SIZE_MAX / count)
        return QJS_INTL_OVERFLOW;
    items = allocator->malloc(allocator->opaque, (size_t)count * sizeof(*items));
    if (!items)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i <= text.length; i++) {
        if (i == text.length || text.data[i] == ' ') {
            QJSIntlBytes item = {text.data + start, i - start};
            int valid = hours ? item.length == 3 && item.data[0] == 'h' &&
                                    ((item.data[1] == '1' &&
                                      (item.data[2] == '1' || item.data[2] == '2')) ||
                                     (item.data[1] == '2' &&
                                      (item.data[2] == '3' || item.data[2] == '4')))
                              : calendar(item);
            if (!valid || index == count)
                goto done;
            items[index++] = item;
            start = i + 1;
        }
    }
    if (index != count)
        goto done;
    qsort(items, count, sizeof(*items), compare);
    for (i = 1; i < count; i++) {
        if (!compare(&items[i - 1], &items[i]))
            goto done;
    }
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, items);
    return status;
}

static int table_open(const QJSIntlData *data, uint32_t identity, unsigned int width,
                      QJSIntlSection *out)
{
    return !qjs_intl_pack_section(&data->pack, identity, out) && out->count &&
           out->length % (width * 4) == 0 && out->length / (width * 4) == out->count;
}

static int key_at(const QJSIntlLocalePreferences *data, const QJSIntlSection *table,
                  unsigned int width, uint32_t row, QJSIntlBytes *out)
{
    uint32_t key = field(table, width, row, 0);
    return (row == 0 || key > field(table, width, row - 1, 0)) &&
           !qjs_intl_data_string(&data->data, key, out);
}

typedef struct PreferenceList {
    uint32_t id, count;
} PreferenceList;

static int compare_lists(const void *left, const void *right)
{
    const PreferenceList *a = left, *b = right;
    return (a->id > b->id) - (a->id < b->id);
}

static QJSIntlStatus validate_lists(const QJSIntlLocalePreferences *data,
                                    const QJSIntlAllocator *allocator,
                                    const QJSIntlSection *table, int hours,
                                    PreferenceList *lists)
{
    uint32_t i;
    int world = 0;

    for (i = 0; i < table->count; i++) {
        QJSIntlBytes key, value;
        uint32_t id = field(table, 3, i, 1), count = field(table, 3, i, 2);
        if (!key_at(data, table, 3, i, &key) || !target(key) ||
            qjs_intl_data_string(&data->data, id, &value) || !count)
            return QJS_INTL_DATA_ERROR;
        world |= key.length == 3 && !memcmp(key.data, "001", 3);
        lists[i] = (PreferenceList){id, count};
    }
    if (!world)
        return QJS_INTL_DATA_ERROR;

    /* Group only referenced lists, not the entire string pool. Each grammar
       gets its own pass: a valid calendar name need not be an hour cycle. */
    qsort(lists, table->count, sizeof(*lists), compare_lists);
    for (i = 0; i < table->count; i++) {
        QJSIntlBytes value;
        QJSIntlStatus status;
        if (i && lists[i].id == lists[i - 1].id) {
            if (lists[i].count != lists[i - 1].count)
                return QJS_INTL_DATA_ERROR;
            continue;
        }
        qjs_intl_data_string(&data->data, lists[i].id, &value);
        status = tokens(allocator, value, lists[i].count, hours);
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_locale_preferences_open(const QJSIntlData *data,
                                               const QJSIntlAllocator *allocator,
                                               QJSIntlLocalePreferences *out)
{
    QJSIntlLocalePreferences result;
    PreferenceList *lists;
    uint32_t i, capacity;
    int world = 0;
    QJSIntlStatus status;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !allocator || !allocator->malloc ||
        !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(&result, 0, sizeof(result));
    result.data = *data;
    if (!table_open(data, QJS_INTL_SECTION_WEEKS, 4, &result.weeks) ||
        !table_open(data, QJS_INTL_SECTION_CALENDAR_PREFERENCES, 3,
                    &result.calendars) ||
        !table_open(data, QJS_INTL_SECTION_HOUR_PREFERENCES, 3, &result.hours) ||
        !table_open(data, QJS_INTL_SECTION_SCRIPT_DIRECTIONS, 2, &result.scripts))
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < result.weeks.count; i++) {
        QJSIntlBytes key;
        uint32_t first = field(&result.weeks, 4, i, 1);
        uint32_t minimum = field(&result.weeks, 4, i, 2);
        uint32_t mask = field(&result.weeks, 4, i, 3);
        if (!key_at(&result, &result.weeks, 4, i, &key) || !region(key) || first < 1 ||
            first > 7 || minimum < 1 || minimum > 7 || mask < 1 || mask > 127)
            return QJS_INTL_DATA_ERROR;
        world |= key.length == 3 && !memcmp(key.data, "001", 3);
    }
    if (!world)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < result.scripts.count; i++) {
        QJSIntlBytes key;
        if (!key_at(&result, &result.scripts, 2, i, &key) || !script(key) ||
            field(&result.scripts, 2, i, 1) > QJS_INTL_DIRECTION_RTL)
            return QJS_INTL_DATA_ERROR;
    }
    capacity = result.calendars.count > result.hours.count
                   ? result.calendars.count : result.hours.count;
    if (sizeof(*lists) > SIZE_MAX / capacity)
        return QJS_INTL_OVERFLOW;
    lists = allocator->malloc(allocator->opaque,
                              (size_t)capacity * sizeof(*lists));
    if (!lists)
        return QJS_INTL_NO_MEMORY;
    status = validate_lists(&result, allocator, &result.calendars, 0, lists);
    if (!status)
        status = validate_lists(&result, allocator, &result.hours, 1, lists);
    allocator->free(allocator->opaque, lists);
    if (!status)
        *out = result;
    return status;
}

static QJSIntlStatus find(const QJSIntlLocalePreferences *data,
                          const QJSIntlSection *table, unsigned int width,
                          QJSIntlBytes key, uint32_t *row)
{
    uint32_t low = 0, high;

    if (!data || !data->data.pack.bytes || (!key.data && key.length))
        return QJS_INTL_INVALID_ARGUMENT;
    high = table->count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        QJSIntlBytes name;
        int order;
        qjs_intl_data_string(&data->data, field(table, width, middle, 0), &name);
        order = compare(&key, &name);
        if (order < 0)
            high = middle;
        else if (order > 0)
            low = middle + 1;
        else {
            *row = middle;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_preference_week(const QJSIntlLocalePreferences *data,
                                       QJSIntlBytes region, QJSIntlWeekInfo *out)
{
    QJSIntlStatus status;
    uint32_t row;
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!data)
        return QJS_INTL_INVALID_ARGUMENT;
    status = find(data, &data->weeks, 4, region, &row);
    if (!status) {
        out->first_day = field(&data->weeks, 4, row, 1);
        out->minimum_days = field(&data->weeks, 4, row, 2);
        out->weekend = field(&data->weeks, 4, row, 3);
    }
    return status;
}

QJSIntlStatus qjs_intl_preference_list(const QJSIntlLocalePreferences *data, int hours,
                                       QJSIntlBytes key, QJSIntlBytes *out,
                                       uint32_t *count)
{
    const QJSIntlSection *table;
    QJSIntlStatus status;
    uint32_t row;
    if (out)
        memset(out, 0, sizeof(*out));
    if (count)
        *count = 0;
    if (!out || !count || !data || (hours != 0 && hours != 1))
        return QJS_INTL_INVALID_ARGUMENT;
    table = hours ? &data->hours : &data->calendars;
    status = find(data, table, 3, key, &row);
    if (!status) {
        qjs_intl_data_string(&data->data, field(table, 3, row, 1), out);
        *count = field(table, 3, row, 2);
    }
    return status;
}

QJSIntlStatus qjs_intl_preference_direction(const QJSIntlLocalePreferences *data,
                                            QJSIntlBytes script, unsigned int *out)
{
    QJSIntlStatus status;
    uint32_t row;
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = QJS_INTL_DIRECTION_UNKNOWN;
    if (!data)
        return QJS_INTL_INVALID_ARGUMENT;
    status = find(data, &data->scripts, 2, script, &row);
    if (!status)
        *out = field(&data->scripts, 2, row, 1);
    return status;
}
