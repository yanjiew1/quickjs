/* BasicFormatMatcher's penalties, legal components and first-candidate ties.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/basic-match.h"
#include <assert.h>
#include <limits.h>

static int score(unsigned int field, int requested, int candidate)
{
    QJSIntlDateTimeComponents a = {0}, b = {0};
    int result = 999;
    a.values[field] = requested;
    b.values[field] = candidate;
    assert(!qjs_intl_datetime_basic_score(&a, &b, &result));
    return result;
}

int main(void)
{
    static const int widths[5][5] = {
        {0, -3, -6, -6, -6}, {-6, 0, -3, -6, -6}, {-8, -6, 0, -3, -6},
        {-8, -8, -6, 0, -3}, {-8, -8, -8, -6, 0}
    };
    static const int zones[6][6] = {
        {0, -3, -1, -4, -120, -120}, {-8, 0, -9, -1, -120, -120},
        {-120, -120, 0, -3, -120, -120}, {-120, -120, -8, 0, -120, -120},
        {-120, -120, -1, -4, 0, -3}, {-120, -120, -9, -1, -8, 0}
    };
    static const int valid[QJS_INTL_DATETIME_COMPONENT_COUNT][6] = {
        {3,4,5,0,0,0}, {3,4,5,0,0,0}, {1,2,0,0,0,0}, {1,2,3,4,5,0},
        {1,2,0,0,0,0}, {3,4,5,0,0,0}, {1,2,0,0,0,0}, {1,2,0,0,0,0},
        {1,2,0,0,0,0}, {1,2,3,0,0,0}, {1,2,3,4,5,6}
    };
    QJSIntlDateTimeComponents a = {0}, b = {0}, candidates[4] = {0};
    unsigned int i, j, field;
    size_t index;
    int result;
    for (i = 1; i <= 5; i++) {
        for (j = 1; j <= 5; j++)
            assert(score(QJS_INTL_DATETIME_MONTH, i, j) == widths[i - 1][j - 1]);
    }
    for (i = 1; i <= 3; i++) {
        for (j = 1; j <= 3; j++)
            assert(score(QJS_INTL_DATETIME_FRACTION, i, j) == widths[i - 1][j - 1]);
    }
    for (i = 1; i <= 6; i++) {
        for (j = 1; j <= 6; j++)
            assert(score(QJS_INTL_DATETIME_ZONE, i, j) == zones[i - 1][j - 1]);
    }
    for (field = 0; field < QJS_INTL_DATETIME_COMPONENT_COUNT; field++) {
        for (i = 0; i < 6 && valid[field][i]; i++) {
            assert(score(field, 0, valid[field][i]) == -20);
            assert(score(field, valid[field][i], 0) == -120);
            assert(!score(field, valid[field][i], valid[field][i]));
        }
        for (i = 1; i <= 6; i++) {
            int found = 0;
            for (j = 0; j < 6; j++)
                found |= valid[field][j] == (int)i;
            if (!found) {
                b.values[field] = i;
                result = 999;
                assert(qjs_intl_datetime_basic_score(&a, &b, &result) == QJS_INTL_INVALID_ARGUMENT && !result);
                assert(qjs_intl_datetime_basic_score(&b, &a, &result) == QJS_INTL_INVALID_ARGUMENT && !result);
                b.values[field] = 0;
            }
        }
    }
    for (field = 0; field < QJS_INTL_DATETIME_COMPONENT_COUNT; field++)
        b.values[field] = valid[field][0];
    assert(!qjs_intl_datetime_basic_score(&a, &b, &result) && result == -20 * QJS_INTL_DATETIME_COMPONENT_COUNT);
    assert(!qjs_intl_datetime_basic_score(&b, &a, &result) && result == -120 * QJS_INTL_DATETIME_COMPONENT_COUNT);
    a.values[QJS_INTL_DATETIME_MONTH] = QJS_INTL_DATETIME_NUMERIC;
    candidates[0].values[QJS_INTL_DATETIME_MONTH] = QJS_INTL_DATETIME_LONG;
    candidates[1].values[QJS_INTL_DATETIME_MONTH] = QJS_INTL_DATETIME_NARROW;
    candidates[2] = candidates[1];
    candidates[3] = a;
    assert(!qjs_intl_datetime_basic_match(&a, candidates, 1, &index) && index == 0);
    assert(!qjs_intl_datetime_basic_match(&a, candidates, 3, &index) && index == 1);
    assert(!qjs_intl_datetime_basic_match(&a, candidates, 4, &index) && index == 3);
    candidates[3].values[QJS_INTL_DATETIME_WEEKDAY] = INT_MAX;
    index = 999;
    assert(qjs_intl_datetime_basic_match(&a, candidates, 4, &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_datetime_basic_match(&a, NULL, 0, &index) == QJS_INTL_UNSUPPORTED && !index);
    assert(qjs_intl_datetime_basic_match(&a, candidates, SIZE_MAX, &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_datetime_basic_match(NULL, candidates, 1, &index) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_datetime_basic_match(&a, NULL, 1, &index) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_datetime_basic_match(&a, candidates, 1, NULL) == QJS_INTL_INVALID_ARGUMENT);
    b.values[0] = INT_MIN;
    assert(qjs_intl_datetime_basic_score(&a, &b, &result) == QJS_INTL_INVALID_ARGUMENT && !result);
    assert(qjs_intl_datetime_basic_score(NULL, &b, &result) == QJS_INTL_INVALID_ARGUMENT && !result);
    assert(qjs_intl_datetime_basic_score(&a, NULL, &result) == QJS_INTL_INVALID_ARGUMENT && !result);
    assert(qjs_intl_datetime_basic_score(&a, &b, NULL) == QJS_INTL_INVALID_ARGUMENT);
    return 0;
}
