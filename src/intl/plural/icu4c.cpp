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
/* ICU supplies all rule/category/range data. The shared exact evaluator avoids
   the precision loss of ICU's public numeric operands without substituting
   native rule data. Resource schemas are checked at formatter construction. */
extern "C" {
#include "icu4c.h"
#include "../icu4c/common.h"
}
#include <unicode/uloc.h>
#include <unicode/ures.h>
#include <unicode/unistr.h>
#include <unicode/unumberformatter.h>
#include <climits>
#include <cstring>
#include <memory>
#include <new>
#include <string>

struct QJSIntlPluralService {
    QJSIntlAllocator allocator;
    QJSICU4CLocales locales;
};

struct QJSIntlPluralFormatter {
    QJSIntlAllocator allocator;
    QJSIntlPluralOptions options;
    QJSIntlBytes rules[QJS_INTL_PLURAL_CATEGORY_COUNT];
    unsigned char ranges[QJS_INTL_PLURAL_CATEGORY_COUNT][QJS_INTL_PLURAL_CATEGORY_COUNT];
    UNumberFormatter *compact;
    char *locale;
    unsigned categories;
};

namespace {
const char *const categories[] = {"zero", "one", "two", "few", "many", "other"};

struct ResourceCloser {
    void operator()(UResourceBundle *bundle) const
    {
        ures_close(bundle);
    }
};
using Resource = std::unique_ptr<UResourceBundle, ResourceCloser>;
struct Failure {
    QJSIntlStatus status;
};

void check(UErrorCode status)
{
    if (U_FAILURE(status))
        throw Failure{qjs_icu4c_status(status)};
}

Resource child(UResourceBundle *parent, const char *key, UResType type)
{
    UErrorCode status = U_ZERO_ERROR;
    Resource result(ures_getByKey(parent, key, nullptr, &status));
    check(status);
    if (!result || ures_getType(result.get()) != type)
        throw Failure{QJS_INTL_DATA_ERROR};
    return result;
}

std::string ascii(const UChar *text, int32_t length)
{
    if (!text || length < 0)
        throw Failure{QJS_INTL_DATA_ERROR};
    std::string result;
    result.reserve(static_cast<size_t>(length));
    for (int32_t i = 0; i < length; i++) {
        if (!text[i] || text[i] > 127)
            throw Failure{QJS_INTL_DATA_ERROR};
        result.push_back(static_cast<char>(text[i]));
    }
    return result;
}

std::string string_value(UResourceBundle *resource)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = 0;
    const UChar *text = ures_getString(resource, &length, &status);
    check(status);
    return ascii(text, length);
}

unsigned category(const char *keyword)
{
    for (unsigned i = 0; i < QJS_INTL_PLURAL_CATEGORY_COUNT; i++) {
        if (keyword && !std::strcmp(keyword, categories[i]))
            return i;
    }
    throw Failure{QJS_INTL_DATA_ERROR};
}

// Missing locale mappings alone are optional. A referenced malformed set or
// a missing bundle is an error, never an invitation to use process defaults.
std::string mapped_set(UResourceBundle *locales, const std::string &locale)
{
    UErrorCode status = U_ZERO_ERROR;
    Resource mapping(ures_getByKey(locales, locale.c_str(), nullptr, &status));
    if (status == U_MISSING_RESOURCE_ERROR)
        return {};
    check(status);
    auto name = string_value(mapping.get());
    if (name.empty())
        throw Failure{QJS_INTL_DATA_ERROR};
    return name;
}

void load_rules(QJSIntlPluralFormatter &formatter)
{
    UErrorCode status = U_ZERO_ERROR;
    Resource bundle(ures_openDirect(nullptr, "plurals", &status));
    check(status);
    auto locales =
        child(bundle.get(),
              formatter.options.type == QJS_INTL_PLURAL_ORDINAL ? "locales_ordinals" : "locales",
              URES_TABLE);
    auto rules = child(bundle.get(), "rules", URES_TABLE);
    std::string locale(formatter.locale), name;

    while (!locale.empty()) {
        name = mapped_set(locales.get(), locale);
        if (!name.empty())
            break;
        std::string parent(locale.size() + 1, '\0');
        status = U_ZERO_ERROR;
        int32_t length = uloc_getParent(locale.c_str(), &parent[0],
                                        static_cast<int32_t>(parent.size()), &status);
        check(status);
        parent.resize(static_cast<size_t>(length));
        locale.swap(parent);
    }
    formatter.categories = 1U << QJS_INTL_PLURAL_OTHER;
    if (name.empty())
        return;
    auto set = child(rules.get(), name.c_str(), URES_TABLE);
    for (int32_t i = 0; i < ures_getSize(set.get()); i++) {
        status = U_ZERO_ERROR;
        Resource rule(ures_getByIndex(set.get(), i, nullptr, &status));
        check(status);
        unsigned index = category(ures_getKey(rule.get()));
        int32_t length = 0;
        const UChar *text = ures_getString(rule.get(), &length, &status);
        check(status);
        // Samples may contain non-ASCII ellipses; only the relation is syntax.
        int32_t end = 0;
        while (end < length && text[end] != '@')
            end++;
        while (end && (text[end - 1] == ' ' || (text[end - 1] >= 9 && text[end - 1] <= 13)))
            end--;
        auto relation = ascii(text, end);
        QJSIntlPluralOperands probe = {"0", 1, 0};
        int matches;
        QJSIntlStatus error =
            qjs_intl_plural_rule_evaluate({relation.data(), relation.size()}, &probe, &matches);
        if (error)
            throw Failure{error};
        char *copy = nullptr;
        error =
            qjs_icu4c_copy_bytes(&formatter.allocator, {relation.data(), relation.size()}, &copy);
        if (error)
            throw Failure{error};
        formatter.rules[index] = {copy, relation.size()};
        formatter.categories |= 1U << index;
    }
}

void load_ranges(QJSIntlPluralFormatter &formatter)
{
    UErrorCode status = U_ZERO_ERROR;
    Resource bundle(ures_openDirect(nullptr, "pluralRanges", &status));
    check(status);
    auto locales = child(bundle.get(), "locales", URES_TABLE);
    auto rules = child(bundle.get(), "rules", URES_TABLE);
    // ICU range mappings use language only, for both cardinal and ordinal.
    char language[ULOC_LANG_CAPACITY];
    uloc_getLanguage(formatter.locale, language, sizeof(language), &status);
    check(status);
    auto name = mapped_set(locales.get(), language);
    std::memset(formatter.ranges, QJS_INTL_PLURAL_OTHER, sizeof(formatter.ranges));
    if (name.empty())
        return;
    auto set = child(rules.get(), name.c_str(), URES_ARRAY);
    bool seen[QJS_INTL_PLURAL_CATEGORY_COUNT][QJS_INTL_PLURAL_CATEGORY_COUNT] = {};
    for (int32_t i = 0; i < ures_getSize(set.get()); i++) {
        status = U_ZERO_ERROR;
        Resource entry(ures_getByIndex(set.get(), i, nullptr, &status));
        check(status);
        if (ures_getType(entry.get()) != URES_ARRAY || ures_getSize(entry.get()) != 3)
            throw Failure{QJS_INTL_DATA_ERROR};
        unsigned triple[3];
        for (int32_t j = 0; j < 3; j++) {
            Resource item(ures_getByIndex(entry.get(), j, nullptr, &status));
            check(status);
            triple[j] = category(string_value(item.get()).c_str());
        }
        if (seen[triple[0]][triple[1]])
            throw Failure{QJS_INTL_DATA_ERROR};
        seen[triple[0]][triple[1]] = true;
        formatter.ranges[triple[0]][triple[1]] = static_cast<unsigned char>(triple[2]);
    }
}

struct Resolution {
    const QJSIntlAllocator &allocator;
    QJSIntlDecimalResult rounded{};
    QJSIntlBytes text{};
    QJSIntlPluralCategory selected = QJS_INTL_PLURAL_OTHER;
    int32_t exponent = 0;

    explicit Resolution(const QJSIntlAllocator &a) : allocator(a) {}
    Resolution(const Resolution &) = delete;
    Resolution &operator=(const Resolution &) = delete;
    ~Resolution()
    {
        qjs_intl_decimal_result_clear(&allocator, &rounded);
    }
};

QJSIntlStatus compact_exponent(const QJSIntlPluralFormatter &formatter, const Resolution &input,
                               int32_t *out)
{
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UFormattedNumber, decltype(&unumf_closeResult)> result(
        unumf_openResult(&status), unumf_closeResult);
    char *text = nullptr;
    QJSIntlDecimal mantissa{};
    QJSIntlStatus error;
    int32_t length;

    *out = 0;
    if (input.text.length > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    unumf_formatDecimal(formatter.compact, input.text.data, static_cast<int32_t>(input.text.length),
                        result.get(), &status);
    length = unumf_resultToDecimalNumber(result.get(), nullptr, 0, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR)
        status = U_ZERO_ERROR;
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    if (length < 0 || length == INT32_MAX)
        return QJS_INTL_OVERFLOW;
    text = static_cast<char *>(
        formatter.allocator.malloc(formatter.allocator.opaque, static_cast<size_t>(length) + 1));
    if (!text)
        return QJS_INTL_NO_MEMORY;
    unumf_resultToDecimalNumber(result.get(), text, length + 1, &status);
    error = qjs_icu4c_status(status);
    if (!error)
        error = qjs_intl_decimal_parse(&formatter.allocator, {text, static_cast<size_t>(length)},
                                       &mantissa);
    if (!error) {
        // This public API serializes the mantissa without the notation exponent.
        int64_t original =
            static_cast<int64_t>(input.rounded.rounded.length) + input.rounded.rounded.exponent;
        int64_t scaled = static_cast<int64_t>(mantissa.length) + mantissa.exponent;
        int64_t exponent = original - scaled;

        if (exponent < INT32_MIN || exponent > INT32_MAX)
            error = QJS_INTL_OVERFLOW;
        else
            *out = static_cast<int32_t>(exponent);
    }
    qjs_intl_decimal_clear(&formatter.allocator, &mantissa);
    formatter.allocator.free(formatter.allocator.opaque, text);
    return error;
}

QJSIntlStatus resolve(const QJSIntlPluralFormatter &formatter,
                      const QJSIntlMathematicalValue *value, Resolution &out)
{
    if (!value || static_cast<unsigned>(value->kind) > QJS_INTL_NEGATIVE_ZERO)
        return QJS_INTL_INVALID_ARGUMENT;
    switch (value->kind) {
    case QJS_INTL_NAN:
        out.text = {"NaN", 3};
        return QJS_INTL_OK;
    case QJS_INTL_POSITIVE_INFINITY:
        out.text = {"Infinity", 8};
        return QJS_INTL_OK;
    case QJS_INTL_NEGATIVE_INFINITY:
        out.text = {"-Infinity", 9};
        return QJS_INTL_OK;
    default:
        break;
    }
    QJSIntlDecimal decimal{};
    QJSIntlStatus error = qjs_intl_decimal_from_value(&formatter.allocator, value, &decimal);

    if (!error)
        error = qjs_intl_decimal_format_numeric(&formatter.allocator, &decimal,
                                                &formatter.options.digits, &out.rounded);
    qjs_intl_decimal_clear(&formatter.allocator, &decimal);
    if (error)
        return error;
    out.text = {out.rounded.text, out.rounded.length};
    if (out.text.length > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    bool zero = out.rounded.rounded.length == 1 && out.rounded.rounded.digits[0] == '0';
    if (!zero) {
        int64_t magnitude =
            static_cast<int64_t>(out.rounded.rounded.length) + out.rounded.rounded.exponent - 1;
        if (magnitude < INT32_MIN || magnitude > INT32_MAX)
            return QJS_INTL_OVERFLOW;
        if (formatter.options.notation == QJS_INTL_PLURAL_SCIENTIFIC)
            out.exponent = static_cast<int32_t>(magnitude);
        else if (formatter.options.notation == QJS_INTL_PLURAL_ENGINEERING) {
            int64_t remainder = magnitude % 3;
            int64_t exponent = magnitude - (remainder < 0 ? remainder + 3 : remainder);
            if (exponent < INT32_MIN || exponent > INT32_MAX)
                return QJS_INTL_OVERFLOW;
            out.exponent = static_cast<int32_t>(exponent);
        } else if (formatter.options.notation == QJS_INTL_PLURAL_COMPACT) {
            error = compact_exponent(formatter, out, &out.exponent);
            if (error)
                return error;
        }
    }
    QJSIntlPluralOperands operands = {out.text.data, out.text.length, out.exponent};
    for (unsigned i = 0; i < QJS_INTL_PLURAL_OTHER; i++) {
        if (!(formatter.categories & (1U << i)))
            continue;
        int matches;
        error = qjs_intl_plural_rule_evaluate(formatter.rules[i], &operands, &matches);
        if (error)
            return error;
        if (matches) {
            out.selected = static_cast<QJSIntlPluralCategory>(i);
            break;
        }
    }
    return QJS_INTL_OK;
}
} // namespace

extern "C" QJSIntlStatus qjs_intl_plural_open(const QJSIntlAllocator *allocator,
                                              const QJSIntlData *data, QJSIntlPluralService **out)
{
    if (out)
        *out = nullptr;
    if (!out || !allocator || !allocator->malloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    auto *service = static_cast<QJSIntlPluralService *>(
        allocator->malloc(allocator->opaque, sizeof(QJSIntlPluralService)));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    std::memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    QJSIntlStatus error = qjs_icu4c_locales_open(allocator, uloc_countAvailable(),
                                                 uloc_getAvailable, &service->locales);

    if (error) {
        qjs_intl_plural_close(service);
        return error;
    }
    *out = service;
    return QJS_INTL_OK;
}

extern "C" void qjs_intl_plural_close(QJSIntlPluralService *service)
{
    if (service) {
        qjs_icu4c_locales_clear(&service->locales);
        service->allocator.free(service->allocator.opaque, service);
    }
}

extern "C" QJSIntlStatus qjs_intl_plural_available(const QJSIntlPluralService *service,
                                                   const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = nullptr;
    if (count)
        *count = 0;
    if (!out || !count || !service)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales.tags;
    *count = service->locales.count;
    return QJS_INTL_OK;
}

extern "C" void qjs_intl_plural_formatter_close(QJSIntlPluralFormatter *formatter)
{
    if (formatter) {
        for (const auto &rule : formatter->rules) {
            if (rule.data)
                formatter->allocator.free(formatter->allocator.opaque,
                                          const_cast<char *>(rule.data));
        }
        unumf_close(formatter->compact);
        if (formatter->locale)
            formatter->allocator.free(formatter->allocator.opaque, formatter->locale);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

extern "C" QJSIntlStatus qjs_intl_plural_formatter_open(const QJSIntlPluralService *service,
                                                        size_t locale,
                                                        const QJSIntlPluralOptions *options,
                                                        QJSIntlPluralFormatter **out)
{
    if (out)
        *out = nullptr;
    if (!service || !out || locale >= service->locales.count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_icu4c_plural_formatter_open(&service->allocator, service->locales.tags[locale],
                                           options, out);
}

extern "C" QJSIntlStatus qjs_icu4c_plural_formatter_open(const QJSIntlAllocator *allocator,
                                                         QJSIntlBytes locale,
                                                         const QJSIntlPluralOptions *options,
                                                         QJSIntlPluralFormatter **out)
{
    QJSIntlPluralFormatter *formatter = nullptr;

    if (out)
        *out = nullptr;
    if (!out || !allocator || !allocator->malloc || !allocator->free || !options ||
        static_cast<unsigned>(options->type) > QJS_INTL_PLURAL_ORDINAL ||
        static_cast<unsigned>(options->notation) > QJS_INTL_PLURAL_COMPACT ||
        options->compact_long > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    QJSIntlStatus error = qjs_intl_decimal_options_validate(&options->digits);

    if (error)
        return error;
    try {
        formatter = static_cast<QJSIntlPluralFormatter *>(
            allocator->malloc(allocator->opaque, sizeof(*formatter)));
        if (!formatter)
            return QJS_INTL_NO_MEMORY;
        std::memset(formatter, 0, sizeof(*formatter));
        formatter->allocator = *allocator;
        formatter->options = *options;
        error = qjs_icu4c_locale_id(allocator, locale, &formatter->locale);
        if (!error) {
            UErrorCode status = U_ZERO_ERROR;
            load_rules(*formatter);
            load_ranges(*formatter);
            if (!error && options->notation == QJS_INTL_PLURAL_COMPACT) {
                const char *ascii = options->compact_long
                                        ? "compact-long precision-unlimited group-off sign-never"
                                        : "compact-short precision-unlimited group-off sign-never";
                auto tokens = icu::UnicodeString::fromUTF8(ascii);
                formatter->compact = unumf_openForSkeletonAndLocale(
                    tokens.getBuffer(), tokens.length(), formatter->locale, &status);
                error = qjs_icu4c_status(status);
            }
        }
    } catch (const Failure &failure) {
        error = failure.status;
    } catch (const std::bad_alloc &) {
        error = QJS_INTL_NO_MEMORY;
    } catch (...) {
        error = QJS_INTL_DATA_ERROR;
    }
    if (error) {
        qjs_intl_plural_formatter_close(formatter);
        return error;
    }
    *out = formatter;
    return QJS_INTL_OK;
}

extern "C" QJSIntlStatus qjs_intl_plural_categories(const QJSIntlPluralFormatter *formatter,
                                                    unsigned *out)
{
    if (out)
        *out = 0;
    if (!out || !formatter)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = formatter->categories;
    return QJS_INTL_OK;
}

extern "C" QJSIntlStatus qjs_intl_plural_select(const QJSIntlPluralFormatter *formatter,
                                                const QJSIntlMathematicalValue *value,
                                                QJSIntlPluralCategory *out)
{
    if (out)
        *out = QJS_INTL_PLURAL_OTHER;
    if (!out || !formatter)
        return QJS_INTL_INVALID_ARGUMENT;
    try {
        Resolution result(formatter->allocator);
        QJSIntlStatus error = resolve(*formatter, value, result);

        if (!error)
            *out = result.selected;
        return error;
    } catch (const std::bad_alloc &) {
        return QJS_INTL_NO_MEMORY;
    } catch (...) {
        return QJS_INTL_DATA_ERROR;
    }
}

extern "C" QJSIntlStatus qjs_intl_plural_select_range(const QJSIntlPluralFormatter *formatter,
                                                      const QJSIntlMathematicalValue *first,
                                                      const QJSIntlMathematicalValue *last,
                                                      QJSIntlPluralCategory *out)
{
    if (out)
        *out = QJS_INTL_PLURAL_OTHER;
    if (!out || !formatter || !first || !last || first->kind == QJS_INTL_NAN ||
        last->kind == QJS_INTL_NAN)
        return QJS_INTL_INVALID_ARGUMENT;
    try {
        Resolution a(formatter->allocator), b(formatter->allocator);
        QJSIntlStatus error = resolve(*formatter, first, a);

        if (!error)
            error = resolve(*formatter, last, b);
        if (error)
            return error;
        if (a.text.length == b.text.length &&
            !std::memcmp(a.text.data, b.text.data, a.text.length)) {
            *out = a.selected;
            return QJS_INTL_OK;
        }
        *out = static_cast<QJSIntlPluralCategory>(formatter->ranges[a.selected][b.selected]);
        return QJS_INTL_OK;
    } catch (const std::bad_alloc &) {
        return QJS_INTL_NO_MEMORY;
    } catch (...) {
        return QJS_INTL_DATA_ERROR;
    }
}
