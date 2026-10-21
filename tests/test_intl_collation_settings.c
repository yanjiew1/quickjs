/* Scalar settings update atomically and preserve independent comparison axes.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/collation-settings.h"
#include <assert.h>
#include <string.h>

static void words(uint8_t *out, const char *text, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++) {
        out[i * 4] = (uint8_t)text[i];
        out[i * 4 + 1] = out[i * 4 + 2] = out[i * 4 + 3] = 0;
    }
}

static QJSIntlStatus apply(QJSIntlCollationPolicy *policy, const char *name, const char *value)
{
    uint8_t key[80], text[100];
    QJSIntlIRInstruction node = {.kind = QJS_INTL_IR_SETTING};
    size_t name_length = strlen(name), value_length = strlen(value) + 1;
    assert(name_length <= sizeof(key) / 4 && value_length <= sizeof(text) / 4);
    words(key, name, name_length);
    words(text, value, value_length);
    node.fields[0] = (QJSIntlIRScalars){key, (uint32_t)name_length};
    node.fields[1] = (QJSIntlIRScalars){text, (uint32_t)value_length};
    return qjs_intl_collation_setting_apply(policy, &node);
}

static int equal(const QJSIntlCollationPolicy *a, const QJSIntlCollationPolicy *b)
{
    return a->strength == b->strength && a->case_first == b->case_first && a->max_variable == b->max_variable &&
        a->shifted == b->shifted && a->backwards_secondary == b->backwards_secondary &&
        a->normalization == b->normalization && a->case_level == b->case_level && a->numeric == b->numeric;
}

int main(void)
{
    QJSIntlCollationPolicy policy, saved;
    const char *const invalid[][2] = {{"strength", "5"}, {"strength", "i"}, {"backwards", "1"},
        {"normalization", "true"}, {"caseLevel", "upper"}, {"caseFirst", "on"},
        {"numericOrdering", "yes"}, {"alternate", "off"}, {"maxVariable", "digit"}};
    unsigned int i;
    qjs_intl_collation_settings_init(&policy);
    assert(policy.strength == QJS_INTL_COLLATION_STRENGTH_TERTIARY &&
           policy.case_first == QJS_INTL_COLLATION_CASE_OFF && policy.max_variable == QJS_INTL_COLLATION_VARIABLE_PUNCT);
    assert(!policy.shifted && !policy.backwards_secondary && !policy.normalization && !policy.case_level && !policy.numeric);
    assert(apply(&policy, "backwards", "1") == QJS_INTL_DATA_ERROR && !policy.backwards_secondary);
    for (i = 1; i <= 4; i++) {
        char value[2] = {(char)('0' + i), 0};
        assert(!apply(&policy, "strength", value) && (unsigned int)policy.strength == i);
    }
    assert(!apply(&policy, "strength", "I") && policy.strength == QJS_INTL_COLLATION_STRENGTH_IDENTICAL);
    assert(!apply(&policy, "alternate", "shifted") && policy.shifted);
    assert(!apply(&policy, "backwards", "2") && policy.backwards_secondary);
    assert(!apply(&policy, "normalization", "on") && policy.normalization);
    assert(!apply(&policy, "caseLevel", "on") && policy.case_level);
    assert(!apply(&policy, "numericOrdering", "on") && policy.numeric);
    assert(!apply(&policy, "caseFirst", "upper") && policy.case_first == QJS_INTL_COLLATION_UPPER_FIRST);
    assert(!apply(&policy, "caseFirst", "lower") && policy.case_first == QJS_INTL_COLLATION_LOWER_FIRST);
    assert(!apply(&policy, "maxVariable", "space") && policy.max_variable == QJS_INTL_COLLATION_VARIABLE_SPACE);
    assert(!apply(&policy, "maxVariable", "symbol") && policy.max_variable == QJS_INTL_COLLATION_VARIABLE_SYMBOL);
    assert(!apply(&policy, "maxVariable", "currency") && policy.max_variable == QJS_INTL_COLLATION_VARIABLE_CURRENCY);
    assert(!apply(&policy, "maxVariable", "punct") && policy.max_variable == QJS_INTL_COLLATION_VARIABLE_PUNCT);
    saved = policy;
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        assert(apply(&policy, invalid[i][0], invalid[i][1]) == QJS_INTL_DATA_ERROR);
        assert(equal(&policy, &saved));
    }
    assert(apply(&policy, "reorder", "Latn") == QJS_INTL_UNSUPPORTED && equal(&policy, &saved));
    assert(apply(&policy, "hiraganaQ", "on") == QJS_INTL_UNSUPPORTED && equal(&policy, &saved));
    assert(apply(&policy, "unknown", "on") == QJS_INTL_UNSUPPORTED && equal(&policy, &saved));
    assert(!apply(&policy, "alternate", "non-ignorable") && !policy.shifted);
    assert(!apply(&policy, "normalization", "off") && !policy.normalization);
    assert(!apply(&policy, "caseLevel", "off") && !policy.case_level);
    assert(!apply(&policy, "numericOrdering", "off") && !policy.numeric);
    assert(!apply(&policy, "caseFirst", "off") && policy.case_first == QJS_INTL_COLLATION_CASE_OFF);
    assert(qjs_intl_collation_setting_apply(&policy, NULL) == QJS_INTL_INVALID_ARGUMENT);
    return 0;
}
