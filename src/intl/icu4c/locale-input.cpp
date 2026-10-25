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
#include "identity.h"
extern "C" {
#include "../common/locale-grammar.h"
}
#include <unicode/ures.h>
#include <unicode/uloc.h>
#include <algorithm>
#include <climits>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {
/* Adapter scratch uses the same allocator as C results. ICU's own allocations
   remain library-owned; no process-global hooks are installed. */
template <class T> struct Scratch {
    using value_type = T;
    QJSIntlAllocator callbacks;
    explicit Scratch(const QJSIntlAllocator &a) : callbacks(a) {}
    template <class U> Scratch(const Scratch<U> &other) : callbacks(other.callbacks) {}
    T *allocate(size_t count)
    {
        if (count > SIZE_MAX / sizeof(T))
            throw std::bad_alloc();
        void *p = callbacks.malloc(callbacks.opaque, count ? count * sizeof(T) : sizeof(T));
        if (!p)
            throw std::bad_alloc();
        return static_cast<T *>(p);
    }
    void deallocate(T *p, size_t)
    {
        if (p)
            callbacks.free(callbacks.opaque, p);
    }
    template <class U> bool operator==(const Scratch<U> &other) const
    {
        return callbacks.opaque == other.callbacks.opaque &&
               callbacks.malloc == other.callbacks.malloc && callbacks.free == other.callbacks.free;
    }
    template <class U> bool operator!=(const Scratch<U> &other) const
    {
        return !(*this == other);
    }
};

using Text = std::basic_string<char, std::char_traits<char>, Scratch<char>>;
template <class T> using Array = std::vector<T, Scratch<T>>;
struct Slice {
    size_t start, length;
};
struct Failure {
    QJSIntlStatus status;
};

static char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}
static bool alpha(char c)
{
    c = lower(c);
    return c >= 'a' && c <= 'z';
}
static bool digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool needs_framing(QJSIntlBytes input)
{
    size_t position = 0, index = 0, keywords = 0;
    bool base = true, unicode = false;

    while (position < input.length) {
        size_t start = position;
        while (position < input.length && input.data[position] != '-')
            position++;
        size_t length = position - start;
        if (length == 1) {
            char singleton = lower(input.data[start]);
            if (singleton == 'x')
                return true; // Legacy keyword serialization also rewrites private-use yes.
            if (singleton == 't')
                return true; // Preserve the original first transformed key.
            base = false;
            unicode = singleton == 'u';
            if (!unicode && ++keywords > 20)
                return true;
        } else if (base && index && (length >= 5 || (length == 4 && digit(input.data[start])))) {
            return true; // Variant framing also avoids legacy POSIX conversion.
        } else if (unicode && length == 3 && lower(input.data[start]) == 'y' &&
                   lower(input.data[start + 1]) == 'e' && lower(input.data[start + 2]) == 's') {
            // Legacy Locale serialization drops even unregistered literal yes.
            return true;
        } else if (unicode && length == 2 && ++keywords > 20) {
            return true; // ICU's monolithic Locale stores at most 25 keywords.
        }
        index++;
        if (position < input.length)
            position++;
    }
    return false;
}

static Array<Slice> split(const Text &text)
{
    Array<Slice> result{Scratch<Slice>(text.get_allocator().callbacks)};
    size_t position = 0;

    while (position < text.size()) {
        size_t start = position;
        while (position < text.size() && text[position] != '-')
            position++;
        result.push_back({start, position - start});
        if (position < text.size())
            position++;
    }
    return result;
}

static Text piece(const Text &text, Slice part)
{
    return Text(text.data() + part.start, part.length, text.get_allocator());
}

static void append(Text &out, const char *text, size_t length)
{
    if (!length)
        return;
    if (!out.empty())
        out.push_back('-');
    out.append(text, length);
}

static void append(Text &out, const Text &text)
{
    append(out, text.data(), text.size());
}

static Text join(const Text &text, const Array<Slice> &parts, size_t start, size_t end)
{
    Text result(text.get_allocator());
    for (size_t i = start; i < end; i++)
        append(result, text.data() + parts[i].start, parts[i].length);
    return result;
}

static bool kind(const Text &text, Slice part, QJSIntlLocaleSubtagKind type)
{
    return qjs_intl_unicode_subtag_well_formed(type, text.data() + part.start, part.length) != 0;
}

static Text component(const QJSIntlAllocator &allocator, const Text &input,
                      QJSICU4CLocaleOperation operation)
{
    char *output = nullptr;
    QJSIntlStatus error =
        qjs_icu4c_locale_component(&allocator, {input.data(), input.size()}, operation, &output);
    if (error)
        throw Failure{error};
    try {
        Text result(output, std::strlen(output), Scratch<char>(allocator));
        allocator.free(allocator.opaque, output);
        return result;
    } catch (...) {
        allocator.free(allocator.opaque, output);
        throw;
    }
}

static void add_sensitive(Array<Text> &out, const char *source, bool compound)
{
    size_t position = 0, index = 0, length = std::strlen(source);
    const QJSIntlAllocator &allocator = out.get_allocator().callbacks;

    while (position < length) {
        size_t start = position;
        while (position < length && source[position] != '_')
            position++;
        if ((!compound || index) &&
            qjs_intl_unicode_subtag_well_formed(QJS_INTL_VARIANT_SUBTAG, source + start,
                                                position - start)) {
            Text value(source + start, position - start, Scratch<char>(allocator));
            for (char &c : value)
                c = lower(c);
            if (std::find(out.begin(), out.end(), value) == out.end())
                out.push_back(std::move(value));
        }
        index++;
        if (position < length)
            position++;
    }
}

static Array<Text> sensitive_variants(const QJSIntlAllocator &allocator)
{
    Array<Text> result{Scratch<Text>(allocator)};
    UErrorCode status = U_ZERO_ERROR;
    icu::LocalUResourceBundlePointer bundle(ures_openDirect(nullptr, "metadata", &status));
    icu::LocalUResourceBundlePointer aliases(
        ures_getByKey(bundle.getAlias(), "alias", nullptr, &status));
    const char *const groups[] = {"language", "variant"};

    for (size_t group = 0; group < 2 && U_SUCCESS(status); group++) {
        icu::LocalUResourceBundlePointer table(
            ures_getByKey(aliases.getAlias(), groups[group], nullptr, &status));
        int32_t count = U_SUCCESS(status) ? ures_getSize(table.getAlias()) : 0;
        for (int32_t i = 0; i < count && U_SUCCESS(status); i++) {
            icu::LocalUResourceBundlePointer row(
                ures_getByIndex(table.getAlias(), i, nullptr, &status));
            const char *key = U_SUCCESS(status) ? ures_getKey(row.getAlias()) : nullptr;
            if (!key) {
                if (U_SUCCESS(status))
                    status = U_INVALID_FORMAT_ERROR;
                break;
            }
            add_sensitive(result, key, group == 0);
        }
    }
    if (U_FAILURE(status))
        throw Failure{status == U_MEMORY_ALLOCATION_ERROR ? QJS_INTL_NO_MEMORY
                                                          : QJS_INTL_DATA_ERROR};
    return result;
}

static Text language(const QJSIntlAllocator &allocator, const Text &input,
                     QJSICU4CLocaleOperation operation)
{
    auto parts = split(input);
    size_t variants = 1;
    if (variants < parts.size() && kind(input, parts[variants], QJS_INTL_SCRIPT_SUBTAG))
        variants++;
    if (variants < parts.size() && kind(input, parts[variants], QJS_INTL_REGION_SUBTAG))
        variants++;
    Text core = join(input, parts, 0, variants);
    Array<Text> saved{Scratch<Text>(allocator)};
    Array<Text> sensitive{Scratch<Text>(allocator)};
    size_t variant_length = 0;
    for (size_t i = variants; i < parts.size(); i++)
        variant_length += parts[i].length + 1;
    bool oversized = variant_length > 179; // ICU MAX_VARIANTS_LENGTH.
    if (oversized)
        sensitive = sensitive_variants(allocator);
    for (size_t i = variants; i < parts.size(); i++) {
        Text variant = piece(input, parts[i]);
        /* POSIX's legacy conversion into u-va-posix is not canonicalization
           (TR35 Legacy Variants). Preserve it outside the legacy Locale form. */
        if (variant == "posix" || (oversized && std::find(sensitive.begin(), sensitive.end(),
                                                          variant) == sensitive.end()))
            saved.push_back(std::move(variant));
        else
            append(core, variant);
    }
    Text canonical = component(allocator, core, operation);
    auto normalized = split(canonical);
    size_t base = 1;
    if (base < normalized.size() && kind(canonical, normalized[base], QJS_INTL_SCRIPT_SUBTAG))
        base++;
    if (base < normalized.size() && kind(canonical, normalized[base], QJS_INTL_REGION_SUBTAG))
        base++;
    for (size_t i = base; i < normalized.size(); i++) {
        if (!kind(canonical, normalized[i], QJS_INTL_VARIANT_SUBTAG))
            throw Failure{QJS_INTL_DATA_ERROR};
        Text variant = piece(canonical, normalized[i]);
        for (char &c : variant)
            c = lower(c);
        saved.push_back(std::move(variant));
    }
    std::sort(saved.begin(), saved.end());
    saved.erase(std::unique(saved.begin(), saved.end()), saved.end());
    Text result = join(canonical, normalized, 0, base);
    for (const Text &variant : saved)
        append(result, variant);
    return result;
}

struct Field {
    Text key, value;
};

static bool transformed_key(const Text &text, Slice token)
{
    return token.length == 2 && alpha(text[token.start]) && digit(text[token.start + 1]);
}

static Text field_value(const QJSIntlAllocator &allocator, char singleton, const Text &key,
                        const Text &value)
{
    if (singleton == 'u' && value == "yes") {
        const char *mapped = uloc_toUnicodeLocaleType(key.c_str(), "yes");

        // Only registered Boolean aliases become true and disappear. Unknown
        // keys and non-Boolean values retain their literal yes type.
        return Text(mapped && std::strcmp(mapped, "true") ? mapped
                    : mapped                              ? ""
                                                          : "yes",
                    Scratch<char>(allocator));
    }
    Text request("und", Scratch<char>(allocator));
    append(request, &singleton, 1);
    append(request, key);
    append(request, value);
    Text canonical = component(allocator, request, QJS_ICU4C_CANONICALIZE);
    auto parts = split(canonical);
    for (size_t i = 1; i + 1 < parts.size(); i++) {
        if (parts[i].length == 1 && canonical[parts[i].start] == singleton) {
            Text actual_key = piece(canonical, parts[i + 1]);
            if (actual_key != key)
                throw Failure{QJS_INTL_DATA_ERROR};
            return join(canonical, parts, i + 2, parts.size());
        }
    }
    throw Failure{QJS_INTL_DATA_ERROR};
}

static Text extension(const QJSIntlAllocator &allocator, char singleton, const Text &text)
{
    auto parts = split(text);
    Array<Text> attributes{Scratch<Text>(allocator)};
    Array<Field> fields{Scratch<Field>(allocator)};
    Text prefix{Scratch<char>(allocator)};
    size_t position = 0;

    if (singleton == 't') {
        while (position < parts.size() && !transformed_key(text, parts[position]))
            position++;
        if (position) {
            prefix = language(allocator, join(text, parts, 0, position), QJS_ICU4C_CANONICALIZE);
            for (char &c : prefix)
                c = lower(c);
        }
    } else {
        while (position < parts.size() && parts[position].length > 2)
            attributes.push_back(piece(text, parts[position++]));
        std::sort(attributes.begin(), attributes.end());
        attributes.erase(std::unique(attributes.begin(), attributes.end()), attributes.end());
    }
    while (position < parts.size()) {
        Text key = piece(text, parts[position++]);
        size_t start = position;
        while (position < parts.size() && parts[position].length > 2)
            position++;
        bool duplicate = std::find_if(fields.begin(), fields.end(), [&](const Field &field) {
                             return field.key == key;
                         }) != fields.end();
        if (!duplicate) {
            Text value = field_value(allocator, singleton, key, join(text, parts, start, position));
            fields.push_back({std::move(key), std::move(value)});
        }
        /* ECMA-402 explicitly keeps the first ukey; first tkey preserves the
           fork's existing policy in the draft/TR35 duplicate-tkey ambiguity. */
    }
    std::sort(fields.begin(), fields.end(),
              [](const Field &a, const Field &b) { return a.key < b.key; });
    Text result{Scratch<char>(allocator)};
    append(result, prefix);
    for (const Text &attribute : attributes)
        append(result, attribute);
    for (const Field &field : fields) {
        append(result, field.key);
        append(result, field.value);
    }
    return result;
}

struct Extension {
    char key;
    Text value;
};

static Text framed(const QJSIntlAllocator &allocator, QJSIntlBytes input,
                   QJSICU4CLocaleOperation operation)
{
    Text text(input.data, input.length, Scratch<char>(allocator));
    for (char &c : text)
        c = lower(c);
    auto parts = split(text);
    size_t base = 1, position;
    while (base < parts.size() && parts[base].length != 1)
        base++;
    Text result = language(allocator, join(text, parts, 0, base), operation);
    Text private_use{Scratch<char>(allocator)};
    Array<Extension> extensions{Scratch<Extension>(allocator)};
    position = base;
    while (position < parts.size()) {
        char singleton = text[parts[position++].start];
        size_t start = position;
        if (singleton == 'x') {
            private_use = join(text, parts, start, parts.size());
            break;
        }
        while (position < parts.size() && parts[position].length != 1)
            position++;
        Text value = join(text, parts, start, position);
        if (singleton == 'u' || singleton == 't')
            value = extension(allocator, singleton, value);
        extensions.push_back({singleton, std::move(value)});
    }
    std::sort(extensions.begin(), extensions.end(),
              [](const Extension &a, const Extension &b) { return a.key < b.key; });
    for (const Extension &item : extensions) {
        append(result, &item.key, 1);
        append(result, item.value);
    }
    if (!private_use.empty()) {
        append(result, "x", 1);
        append(result, private_use);
    }
    return result;
}
} // namespace

extern "C" QJSIntlStatus qjs_icu4c_locale_identity(const QJSIntlAllocator *allocator,
                                                   QJSIntlBytes input,
                                                   QJSICU4CLocaleOperation operation, char **out)
{
    if (out)
        *out = nullptr;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free ||
        static_cast<unsigned int>(operation) > QJS_ICU4C_MINIMIZE)
        return QJS_INTL_INVALID_ARGUMENT;
    if (input.length > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    if (!qjs_intl_unicode_locale_well_formed(input.data, input.length))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!needs_framing(input))
        return qjs_icu4c_locale_component(allocator, input, operation, out);
    try {
        Text result = framed(*allocator, input, operation);
        char *copy = static_cast<char *>(allocator->malloc(allocator->opaque, result.size() + 1));
        if (!copy)
            return QJS_INTL_NO_MEMORY;
        std::memcpy(copy, result.data(), result.size());
        copy[result.size()] = 0;
        *out = copy;
        return QJS_INTL_OK;
    } catch (const Failure &failure) {
        return failure.status;
    } catch (const std::bad_alloc &) {
        return QJS_INTL_NO_MEMORY;
    } catch (...) {
        return QJS_INTL_DATA_ERROR;
    }
}
