/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-settings.h"

void qjs_intl_collation_settings_init(QJSIntlCollationPolicy *out)
{
    if (out)
        *out = (QJSIntlCollationPolicy){.strength = QJS_INTL_COLLATION_STRENGTH_TERTIARY,
            .case_first = QJS_INTL_COLLATION_CASE_OFF, .max_variable = QJS_INTL_COLLATION_VARIABLE_PUNCT};
}

static int boolean(const QJSIntlIRScalars *value, int *out)
{
    if (qjs_intl_ir_ascii_equal(value, "on", 1))
        *out = 1;
    else if (qjs_intl_ir_ascii_equal(value, "off", 1))
        *out = 0;
    else
        return 0;
    return 1;
}

QJSIntlStatus qjs_intl_collation_setting_apply(QJSIntlCollationPolicy *policy,
                                               const QJSIntlIRInstruction *node)
{
    QJSIntlCollationPolicy next;
    const QJSIntlIRScalars *name, *value;
    int valid = 1;
    if (!policy || !node || node->kind != QJS_INTL_IR_SETTING || node->auxiliary ||
        !node->fields[0].bytes || !node->fields[0].count ||
        !node->fields[1].bytes || !node->fields[1].count || node->fields[2].count)
        return QJS_INTL_INVALID_ARGUMENT;
    name = node->fields;
    value = node->fields + 1;
    next = *policy;
    if (qjs_intl_ir_ascii_equal(name, "strength", 0)) {
        uint32_t code = qjs_intl_ir_scalar(value, 0);
        if (value->count == 2 && !qjs_intl_ir_scalar(value, 1) && code >= '1' && code <= '4')
            next.strength = (QJSIntlCollationStrength)(code - '0');
        else if (qjs_intl_ir_ascii_equal(value, "I", 1))
            next.strength = QJS_INTL_COLLATION_STRENGTH_IDENTICAL;
        else
            valid = 0;
    } else if (qjs_intl_ir_ascii_equal(name, "alternate", 0)) {
        if (qjs_intl_ir_ascii_equal(value, "shifted", 1))
            next.shifted = 1;
        else if (qjs_intl_ir_ascii_equal(value, "non-ignorable", 1))
            next.shifted = 0;
        else
            valid = 0;
    } else if (qjs_intl_ir_ascii_equal(name, "backwards", 0)) {
        valid = qjs_intl_ir_ascii_equal(value, "2", 1);
        next.backwards_secondary = 1;
    } else if (qjs_intl_ir_ascii_equal(name, "normalization", 0)) {
        valid = boolean(value, &next.normalization);
    } else if (qjs_intl_ir_ascii_equal(name, "caseLevel", 0)) {
        valid = boolean(value, &next.case_level);
    } else if (qjs_intl_ir_ascii_equal(name, "numericOrdering", 0)) {
        valid = boolean(value, &next.numeric);
    } else if (qjs_intl_ir_ascii_equal(name, "caseFirst", 0)) {
        if (qjs_intl_ir_ascii_equal(value, "off", 1))
            next.case_first = QJS_INTL_COLLATION_CASE_OFF;
        else if (qjs_intl_ir_ascii_equal(value, "upper", 1))
            next.case_first = QJS_INTL_COLLATION_UPPER_FIRST;
        else if (qjs_intl_ir_ascii_equal(value, "lower", 1))
            next.case_first = QJS_INTL_COLLATION_LOWER_FIRST;
        else
            valid = 0;
    } else if (qjs_intl_ir_ascii_equal(name, "maxVariable", 0)) {
        static const char *const names[] = {"space", "punct", "symbol", "currency"};
        unsigned int i;
        for (i = 0; i < 4; i++) {
            if (qjs_intl_ir_ascii_equal(value, names[i], 1)) {
                next.max_variable = (QJSIntlCollationMaxVariable)i;
                break;
            }
        }
        valid = i < 4;
    } else {
        return QJS_INTL_UNSUPPORTED;
    }
    if (!valid)
        return QJS_INTL_DATA_ERROR;
    *policy = next;
    return QJS_INTL_OK;
}
