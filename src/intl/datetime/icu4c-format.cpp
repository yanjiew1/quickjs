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
/* Only this provider adapter needs the public ICU C++ formatter API. It
   permits proleptic Gregorian subclasses without casting opaque C handles. */
extern "C" {
#include "icu4c.h"
#include "basic-match.h"
#include "pattern-info.h"
#include "../icu4c/common.h"
#include "../common/range-compose.h"
#include "../../calendar/calendar.h"
#include "../../calendar/data.h"
}
#include <unicode/datefmt.h>
#include <unicode/dtfmtsym.h>
#include <unicode/dtitvfmt.h>
#include <unicode/dtitvinf.h>
#include <unicode/dtptngen.h>
#include <unicode/fpositer.h>
#include <unicode/gregocal.h>
#include <unicode/smpdtfmt.h>
#include <unicode/strenum.h>
#include <unicode/timezone.h>
#include <unicode/udat.h>
#include <unicode/ustring.h>
#include <climits>
#include <cfloat>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>

struct ICUDateRecord {
    icu::SimpleDateFormat *single;
    icu::DateIntervalFormat *range;
    QJSIntlDateTimeComponents components;
    bool numeric_era;
};

struct QJSIntlDateTimeFormatter {
    QJSIntlAllocator allocator;
    QJSIntlDateTimeOptions options;
    char *locale;
    char calendar[32], system[32], zone[256];
    ICUDateRecord records[QJS_INTL_DATE_VALUE_KIND_COUNT];
    char *fallback_storage;
    QJSIntlPairPattern fallback;
    int ranges_prepared;
};

namespace {
using icu::UnicodeString;

template <class T> using Owner = std::unique_ptr<T>;

constexpr unsigned field_bit(unsigned field)
{
    return 1U << field;
}
constexpr unsigned date_fields =
    field_bit(QJS_INTL_DATETIME_WEEKDAY) | field_bit(QJS_INTL_DATETIME_YEAR) |
    field_bit(QJS_INTL_DATETIME_MONTH) | field_bit(QJS_INTL_DATETIME_DAY);
constexpr unsigned time_fields =
    field_bit(QJS_INTL_DATETIME_DAY_PERIOD) | field_bit(QJS_INTL_DATETIME_HOUR) |
    field_bit(QJS_INTL_DATETIME_MINUTE) | field_bit(QJS_INTL_DATETIME_SECOND) |
    field_bit(QJS_INTL_DATETIME_FRACTION);
constexpr unsigned era_field = field_bit(QJS_INTL_DATETIME_ERA);

QJSIntlStatus utf8(const QJSIntlAllocator &allocator, const UnicodeString &input, char **out)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t length;

    *out = nullptr;
    if (input.isBogus())
        return QJS_INTL_NO_MEMORY;
    u_strToUTF8(nullptr, 0, &length, input.getBuffer(), input.length(), &status);
    if (status == U_BUFFER_OVERFLOW_ERROR)
        status = U_ZERO_ERROR;
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    if (length == INT32_MAX)
        return QJS_INTL_OVERFLOW;
    char *result =
        static_cast<char *>(allocator.malloc(allocator.opaque, static_cast<size_t>(length) + 1));
    if (!result)
        return QJS_INTL_NO_MEMORY;
    u_strToUTF8(result, length + 1, nullptr, input.getBuffer(), input.length(), &status);
    if (U_FAILURE(status)) {
        allocator.free(allocator.opaque, result);
        return qjs_icu4c_status(status);
    }
    *out = result;
    return QJS_INTL_OK;
}

/* Metadata projection uses the same reviewed LDML reader as the native
   provider. ICU remains responsible for names, pattern generation and dates. */
QJSIntlStatus project(const QJSIntlAllocator &allocator, UnicodeString &pattern,
                      QJSIntlDateTimeComponents *out, unsigned *hour_symbol = nullptr)
{
    QJSIntlDatePattern compiled{};
    QJSIntlDatePatternInfo info{};
    char *source = nullptr;
    QJSIntlStatus error = utf8(allocator, pattern, &source);

    if (error)
        return error;
    error = qjs_intl_date_pattern_compile(&allocator, {source, std::strlen(source)}, &compiled);
    allocator.free(allocator.opaque, source);
    bool fix_year = !error && qjs_intl_date_pattern_fix_calendar_year(&compiled);
    if (!error)
        error = qjs_intl_date_pattern_info(&compiled, &info);
    qjs_intl_date_pattern_clear(&allocator, &compiled);
    if (error)
        return error;
    *out = info.components;
    if (hour_symbol)
        *hour_symbol = info.hour_symbol;
    /* Apply the CLDR Y-with-month workaround to the executable copy as well.
       ECMA numeric years have no yyyy minimum padding; yy remains two-digit. */
    bool quoted = false;
    for (int32_t i = 0; i < pattern.length();) {
        UChar c = pattern.charAt(i);
        if (c == '\'') {
            if (i + 1 < pattern.length() && pattern.charAt(i + 1) == '\'')
                i += 2;
            else {
                quoted = !quoted;
                i++;
            }
            continue;
        }
        if (quoted) {
            i++;
            continue;
        }
        int32_t end = i + 1;
        while (end < pattern.length() && pattern.charAt(end) == c)
            end++;
        if (fix_year && c == 'Y') {
            for (int32_t at = i; at < end; at++)
                pattern.setCharAt(at, 'y');
            c = 'y';
        }
        if ((c == 'y' && end - i > 2) || (c == 'r' && end - i > 1)) {
            pattern.remove(i + 1, end - i - 1);
            end = i + 1;
        }
        i = end;
    }
    return pattern.isBogus() ? QJS_INTL_NO_MEMORY : QJS_INTL_OK;
}

void repeat(UnicodeString &out, char code, int count)
{
    while (count-- > 0)
        out.append(static_cast<UChar>(code));
}

UnicodeString skeleton(const QJSIntlDateTimeComponents &components, QJSIntlDateHourCycle cycle)
{
    UnicodeString result;
    const int *v = components.values;
    auto width = [](int value) {
        return value == QJS_INTL_DATETIME_LONG ? 4 : value == QJS_INTL_DATETIME_NARROW ? 5 : 3;
    };

    if (v[QJS_INTL_DATETIME_WEEKDAY])
        repeat(result, 'E', width(v[QJS_INTL_DATETIME_WEEKDAY]));
    if (v[QJS_INTL_DATETIME_ERA])
        repeat(result, 'G', width(v[QJS_INTL_DATETIME_ERA]));
    if (v[QJS_INTL_DATETIME_YEAR])
        repeat(result, 'y', v[QJS_INTL_DATETIME_YEAR] == QJS_INTL_DATETIME_TWO_DIGIT ? 2 : 1);
    if (v[QJS_INTL_DATETIME_MONTH]) {
        int value = v[QJS_INTL_DATETIME_MONTH];
        repeat(result, 'M',
               value == QJS_INTL_DATETIME_TWO_DIGIT ? 2
               : value == QJS_INTL_DATETIME_NUMERIC ? 1
                                                    : width(value));
    }
    if (v[QJS_INTL_DATETIME_DAY])
        repeat(result, 'd', v[QJS_INTL_DATETIME_DAY] == QJS_INTL_DATETIME_TWO_DIGIT ? 2 : 1);
    if (v[QJS_INTL_DATETIME_DAY_PERIOD])
        repeat(result, 'B', width(v[QJS_INTL_DATETIME_DAY_PERIOD]));
    if (v[QJS_INTL_DATETIME_HOUR])
        repeat(result, "KhHk"[cycle],
               v[QJS_INTL_DATETIME_HOUR] == QJS_INTL_DATETIME_TWO_DIGIT ? 2 : 1);
    if (v[QJS_INTL_DATETIME_MINUTE])
        repeat(result, 'm', v[QJS_INTL_DATETIME_MINUTE] == QJS_INTL_DATETIME_TWO_DIGIT ? 2 : 1);
    if (v[QJS_INTL_DATETIME_SECOND])
        repeat(result, 's', v[QJS_INTL_DATETIME_SECOND] == QJS_INTL_DATETIME_TWO_DIGIT ? 2 : 1);
    repeat(result, 'S', v[QJS_INTL_DATETIME_FRACTION]);
    switch (v[QJS_INTL_DATETIME_ZONE]) {
    case QJS_INTL_DATETIME_ZONE_SHORT:
        repeat(result, 'z', 1);
        break;
    case QJS_INTL_DATETIME_ZONE_LONG:
        repeat(result, 'z', 4);
        break;
    case QJS_INTL_DATETIME_ZONE_SHORT_OFFSET:
        repeat(result, 'O', 1);
        break;
    case QJS_INTL_DATETIME_ZONE_LONG_OFFSET:
        repeat(result, 'O', 4);
        break;
    case QJS_INTL_DATETIME_ZONE_SHORT_GENERIC:
        repeat(result, 'v', 1);
        break;
    case QJS_INTL_DATETIME_ZONE_LONG_GENERIC:
        repeat(result, 'v', 4);
        break;
    }
    return result;
}

QJSIntlStatus set_hour_cycle(UnicodeString &pattern, unsigned hour, QJSIntlDateHourCycle cycle)
{
    bool quoted = false;

    for (int32_t at = 0; at < pattern.length(); at++) {
        UChar c = pattern.charAt(at);
        if (c == '\'') {
            if (at + 1 < pattern.length() && pattern.charAt(at + 1) == '\'')
                at++;
            else
                quoted = !quoted;
        } else if (!quoted && hour && c == hour) {
            pattern.setCharAt(at, "KhHk"[cycle]);
        }
    }
    return pattern.isBogus() ? QJS_INTL_NO_MEMORY : QJS_INTL_OK;
}

QJSIntlStatus choose_pattern(QJSIntlDateTimeFormatter &formatter,
                             icu::DateTimePatternGenerator &generator,
                             const QJSIntlDateTimeComponents &request, UnicodeString &pattern,
                             QJSIntlDateTimeComponents &components)
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString requested = skeleton(request, formatter.options.hour_cycle);

    if (formatter.options.best_fit) {
        unsigned hour = 0;
        pattern = generator.getBestPattern(requested, UDATPG_MATCH_HOUR_FIELD_LENGTH, status);
        if (U_FAILURE(status))
            return qjs_icu4c_status(status);
        QJSIntlStatus error = project(formatter.allocator, pattern, &components, &hour);
        if (error)
            return error;
        if (formatter.options.hour_cycle >= QJS_INTL_DATE_H23 &&
            request.values[QJS_INTL_DATETIME_HOUR] &&
            request.values[QJS_INTL_DATETIME_DAY_PERIOD] &&
            !components.values[QJS_INTL_DATETIME_DAY_PERIOD]) {
            /* ICU drops B with H/k even when explicitly requested. Retain an
               explicit twelve-hour source's period/layout and change only
               its hour origin; never manufacture translated period text. */
            requested = skeleton(request, QJS_INTL_DATE_H12);
            pattern = generator.getBestPattern(requested, UDATPG_MATCH_HOUR_FIELD_LENGTH, status);
            if (U_FAILURE(status))
                return qjs_icu4c_status(status);
            error = project(formatter.allocator, pattern, &components, &hour);
            if (!error)
                error = set_hour_cycle(pattern, hour, formatter.options.hour_cycle);
        }
        return error;
    }
    /* BasicFormatMatcher scores actual source component vectors. The ICU
       best-fit generator cannot substitute for that prescribed algorithm. */
    int best_score = INT_MIN;
    auto consider = [&](UnicodeString candidate) -> QJSIntlStatus {
        // ICU enumerates inherited skeleton keys whose direct pattern is empty.
        if (candidate.isEmpty())
            return QJS_INTL_OK;
        QJSIntlDateTimeComponents facts{};
        unsigned hour = 0;
        QJSIntlStatus error = project(formatter.allocator, candidate, &facts, &hour);
        int score;

        if (error == QJS_INTL_UNSUPPORTED)
            return QJS_INTL_OK;
        if (error)
            return error;
        bool twelve = formatter.options.hour_cycle <= QJS_INTL_DATE_H12;
        if (hour && ((hour == 'h' || hour == 'K') != twelve) &&
            !facts.values[QJS_INTL_DATETIME_DAY_PERIOD])
            return QJS_INTL_OK;
        error = qjs_intl_datetime_basic_score(&request, &facts, &score);
        if (!error && score > best_score) {
            pattern = candidate;
            // Explicit b/B layouts retain their period across clock families.
            // Implicit AM/PM layouts were restricted to their own family above.
            error = set_hour_cycle(pattern, hour, formatter.options.hour_cycle);
            if (error)
                return error;
            components = facts;
            best_score = score;
        }
        return error;
    };
    Owner<icu::StringEnumeration> sources(generator.getSkeletons(status));
    while (U_SUCCESS(status)) {
        const UnicodeString *key = sources->snext(status);
        if (!key)
            break;
        QJSIntlStatus error = consider(generator.getPatternForSkeleton(*key));
        if (error)
            return error;
    }
    /* Complete the mandatory presence subsets with ICU-generated formats.
       They are candidates too, never a shortcut around the basic score. */
    static const char *const dates[] = {"M",    "yM",     "Md",      "yMd",     "yMMM",
                                        "MMMd", "yMMMd",  "yMMMM",   "MMMMd",   "yMMMMd",
                                        "Ed",   "yMEd",   "yMMMEd",  "yMMMMEd", "GyM",
                                        "GyMd", "GyMMMd", "GyMMMMd", "GyMMMEd", "GyMMMMEd"};
    /* Include specific, offset and generic zone families. A short-name-only
       catalog can tie an offset request with a no-zone format and silently
       discard its zone before the formatter is even constructed. */
    static const char *const clocks[] = {
        "j",      "jm",      "jms",   "jmz",    "jmsz",   "jmzzzz",  "jmszzzz", "jmO", "jmsO",
        "jmOOOO", "jmsOOOO", "jmv",   "jmsv",   "jmvvvv", "jmsvvvv", "B",       "Bh",  "Bhm",
        "Bhms",   "jmsS",    "jmsSS", "jmsSSS", "BhmsS",  "BhmsSS",  "BhmsSSS",
    };
    auto generated = [&](const UnicodeString &key) -> QJSIntlStatus {
        return consider(generator.getBestPattern(key, status));
    };
    for (const char *date : dates) {
        UnicodeString key = UnicodeString::fromUTF8(date);
        QJSIntlStatus error = generated(key);
        if (error)
            return error;
        for (const char *clock : clocks) {
            UnicodeString combined(key);
            combined.append(UnicodeString::fromUTF8(clock));
            error = generated(combined);
            if (error)
                return error;
        }
    }
    for (const char *clock : clocks) {
        QJSIntlStatus error = generated(UnicodeString::fromUTF8(clock));
        if (error)
            return error;
    }
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    return best_score == INT_MIN ? QJS_INTL_UNSUPPORTED : QJS_INTL_OK;
}

Owner<icu::TimeZone> selected_zone(const QJSIntlDateTimeFormatter &formatter, bool plain)
{
    UnicodeString id;

    if (plain)
        id = UnicodeString::fromUTF8("UTC");
    else {
        if (formatter.zone[0] == '+' || formatter.zone[0] == '-')
            id = UnicodeString::fromUTF8("GMT");
        id.append(UnicodeString::fromUTF8(formatter.zone));
    }
    return Owner<icu::TimeZone>(icu::TimeZone::createTimeZone(id));
}

/* A format-only projection, never used for parsing or field-to-time
   arithmetic. GregorianCalendar supplies the actual local day, clock, weekday
   and zone offsets. Only calendar labels are replaced. Adopting this class in
   the formatter is essential: ICU ignores a different concrete Calendar type
   passed merely as a formatting argument. */
class LunarFormatCalendar final : public icu::GregorianCalendar {
  public:
    LunarFormatCalendar(const icu::Calendar &source, QJSCalendarId id, UErrorCode &status)
        : icu::GregorianCalendar(source.getTimeZone(), status), id_(id)
    {
        Owner<icu::Calendar> reference_calendar(source.clone());
        if (!reference_calendar) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        setGregorianChange(-DBL_MAX, status);
        reference_calendar->setTime(0, status);
        QJSCalendarDate reference{};
        int error = qjs_calendar_official_date(id_, 0, &reference);
        if (error) {
            status = U_INVALID_FORMAT_ERROR;
            return;
        }
        int cyclic = reference_calendar->get(UCAL_YEAR, status);
        cycle_start_ = reference.year - (cyclic - 1);
        cycle_era_ = reference_calendar->get(UCAL_ERA, status);
    }

    LunarFormatCalendar(const LunarFormatCalendar &other) = default;

    LunarFormatCalendar *clone() const override
    {
        return new LunarFormatCalendar(*this);
    }
    const char *getType() const override
    {
        return id_ == QJS_CAL_CHINESE ? "chinese" : "dangi";
    }

    // Only the completed per-call formatter uses the singleton era symbol.
    void use_era_symbol_index()
    {
        internalSet(UCAL_ERA, 0);
    }

  protected:
    void computeFields(UErrorCode &status) override
    {
        icu::GregorianCalendar::computeFields(status);
        if (U_FAILURE(status))
            return;
        int64_t day = static_cast<int64_t>(internalGet(UCAL_JULIAN_DAY)) - 2440588;
        QJSCalendarDate date{};
        int error = qjs_calendar_official_date(id_, day, &date);
        if (error == QJS_CAL_UNSUPPORTED)
            error = qjs_calendar_from_epoch_day(id_, day, &date);
        if (error) {
            status = error == QJS_CAL_MEMORY ? U_MEMORY_ALLOCATION_ERROR : U_INVALID_FORMAT_ERROR;
            return;
        }
        int delta = date.year - cycle_start_;
        int cycles = delta / 60 - (delta % 60 < 0);
        int nominal = (date.month_code[1] - '0') * 10 + date.month_code[2] - '0';
        internalSet(UCAL_EXTENDED_YEAR, date.year);
        internalSet(UCAL_ERA, cycle_era_ + cycles);
        internalSet(UCAL_YEAR, delta - cycles * 60 + 1);
        internalSet(UCAL_MONTH, nominal - 1);
        internalSet(UCAL_ORDINAL_MONTH, date.month - 1);
        internalSet(UCAL_IS_LEAP_MONTH, date.month_code[3] == 'L');
        internalSet(UCAL_DATE, date.day);
        internalSet(UCAL_DAY_OF_YEAR, date.day_of_year);
    }

  private:
    QJSCalendarId id_;
    int cycle_start_ = 0, cycle_era_ = 0;
};

QJSIntlStatus install_record(QJSIntlDateTimeFormatter &formatter, unsigned kind,
                             UnicodeString pattern, const QJSIntlDateTimeComponents &components)
{
    UErrorCode status = U_ZERO_ERROR;
    icu::Locale locale(formatter.locale);
    bool plain =
        kind >= QJS_INTL_DATE_VALUE_PLAIN_DATE && kind <= QJS_INTL_DATE_VALUE_PLAIN_DATE_TIME;
    Owner<icu::TimeZone> zone = selected_zone(formatter, plain);
    Owner<icu::SimpleDateFormat> single(new icu::SimpleDateFormat(pattern, locale, status));

    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    if (!zone || !single)
        return QJS_INTL_NO_MEMORY;
    single->setTimeZone(*zone);
    Owner<icu::Calendar> calendar(single->getCalendar()->clone());
    if (!calendar)
        return QJS_INTL_NO_MEMORY;
    if (auto *gregorian = dynamic_cast<icu::GregorianCalendar *>(calendar.get())) {
        gregorian->setGregorianChange(-DBL_MAX, status);
        if (U_FAILURE(status))
            return qjs_icu4c_status(status);
    }
    const char *type = calendar->getType();
    if (!std::strcmp(type, "chinese") || !std::strcmp(type, "dangi")) {
        QJSCalendarId id = !std::strcmp(type, "chinese") ? QJS_CAL_CHINESE : QJS_CAL_DANGI;
        Owner<icu::Calendar> projected(new LunarFormatCalendar(*calendar, id, status));
        if (!projected)
            return QJS_INTL_NO_MEMORY;
        if (U_FAILURE(status))
            return qjs_icu4c_status(status);
        calendar = std::move(projected);
    }
    bool lunar = dynamic_cast<const LunarFormatCalendar *>(calendar.get());
    single->adoptCalendar(calendar.release());
    formatter.records[kind].numeric_era = lunar && components.values[QJS_INTL_DATETIME_ERA];
    formatter.records[kind].components = components;
    formatter.records[kind].single = single.release();
    return QJS_INTL_OK;
}

unsigned required_mask(QJSIntlDateRequired required)
{
    switch (required) {
    case QJS_INTL_DATE_REQUIRE_DATE:
        return date_fields;
    case QJS_INTL_DATE_REQUIRE_TIME:
        return time_fields;
    case QJS_INTL_DATE_REQUIRE_YEAR_MONTH:
        return field_bit(QJS_INTL_DATETIME_YEAR) | field_bit(QJS_INTL_DATETIME_MONTH);
    case QJS_INTL_DATE_REQUIRE_MONTH_DAY:
        return field_bit(QJS_INTL_DATETIME_MONTH) | field_bit(QJS_INTL_DATETIME_DAY);
    default:
        return date_fields | time_fields;
    }
}

unsigned default_mask(QJSIntlDateDefaults defaults)
{
    switch (defaults) {
    case QJS_INTL_DATE_DEFAULT_DATE:
        return field_bit(QJS_INTL_DATETIME_YEAR) | field_bit(QJS_INTL_DATETIME_MONTH) |
               field_bit(QJS_INTL_DATETIME_DAY);
    case QJS_INTL_DATE_DEFAULT_TIME:
        return field_bit(QJS_INTL_DATETIME_HOUR) | field_bit(QJS_INTL_DATETIME_MINUTE) |
               field_bit(QJS_INTL_DATETIME_SECOND);
    case QJS_INTL_DATE_DEFAULT_YEAR_MONTH:
        return field_bit(QJS_INTL_DATETIME_YEAR) | field_bit(QJS_INTL_DATETIME_MONTH);
    case QJS_INTL_DATE_DEFAULT_MONTH_DAY:
        return field_bit(QJS_INTL_DATETIME_MONTH) | field_bit(QJS_INTL_DATETIME_DAY);
    default:
        return default_mask(QJS_INTL_DATE_DEFAULT_DATE) | default_mask(QJS_INTL_DATE_DEFAULT_TIME);
    }
}

QJSIntlStatus get_record(QJSIntlDateTimeFormatter &formatter,
                         icu::DateTimePatternGenerator &generator, unsigned kind,
                         QJSIntlDateRequired required, QJSIntlDateDefaults defaults, bool relevant)
{
    const auto &options = formatter.options.components;
    auto request = relevant ? QJSIntlDateTimeComponents{} : options;
    unsigned fields = required_mask(required), present = qjs_intl_datetime_component_mask(&options);

    if (relevant &&
        (required == QJS_INTL_DATE_REQUIRE_DATE || required == QJS_INTL_DATE_REQUIRE_YEAR_MONTH ||
         required == QJS_INTL_DATE_REQUIRE_ANY))
        request.values[QJS_INTL_DATETIME_ERA] = options.values[QJS_INTL_DATETIME_ERA];
    for (unsigned i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if ((fields >> i) & 1)
            request.values[i] = options.values[i];
    if (!(present & fields)) {
        if (relevant && (present & (date_fields | time_fields)))
            return QJS_INTL_OK; // Null value-kind record, not a failed provider.
        fields = default_mask(defaults);
        for (unsigned i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
            if ((fields >> i) & 1)
                request.values[i] = QJS_INTL_DATETIME_NUMERIC;
        if (defaults == QJS_INTL_DATE_DEFAULT_ZONED && !request.values[QJS_INTL_DATETIME_ZONE])
            request.values[QJS_INTL_DATETIME_ZONE] = QJS_INTL_DATETIME_ZONE_SHORT;
    }
    UnicodeString pattern;
    QJSIntlDateTimeComponents components{};
    QJSIntlStatus error = choose_pattern(formatter, generator, request, pattern, components);

    return error ? error : install_record(formatter, kind, pattern, components);
}

QJSIntlStatus select_records(QJSIntlDateTimeFormatter &formatter,
                             icu::DateTimePatternGenerator &generator)
{
    const auto &options = formatter.options;
    QJSIntlStatus error;

    if (options.date_style || options.time_style) {
        auto date_style = options.date_style
                              ? static_cast<icu::DateFormat::EStyle>(options.date_style - 1)
                              : icu::DateFormat::kNone;
        auto time_style = options.time_style
                              ? static_cast<icu::DateFormat::EStyle>(options.time_style - 1)
                              : icu::DateFormat::kNone;
        Owner<icu::DateFormat> style(icu::DateFormat::createDateTimeInstance(
            date_style, time_style, icu::Locale(formatter.locale)));
        auto *simple = dynamic_cast<icu::SimpleDateFormat *>(style.get());
        UnicodeString pattern;
        QJSIntlDateTimeComponents components{};

        if (!simple)
            return style ? QJS_INTL_DATA_ERROR : QJS_INTL_NO_MEMORY;
        simple->toPattern(pattern);
        error = project(formatter.allocator, pattern, &components);
        if (!error)
            error = install_record(formatter, QJS_INTL_DATE_VALUE_DATE, pattern, components);
        if (!error)
            error = install_record(formatter, QJS_INTL_DATE_VALUE_INSTANT, pattern, components);
        if (error)
            return error;
        for (unsigned kind = QJS_INTL_DATE_VALUE_PLAIN_DATE;
             kind <= QJS_INTL_DATE_VALUE_PLAIN_DATE_TIME; kind++) {
            unsigned mask = 0;

            switch (kind) {
            case QJS_INTL_DATE_VALUE_PLAIN_DATE:
                if (options.date_style)
                    mask = date_fields | era_field;
                break;
            case QJS_INTL_DATE_VALUE_PLAIN_YEAR_MONTH:
                if (options.date_style)
                    mask = era_field | field_bit(QJS_INTL_DATETIME_YEAR) |
                           field_bit(QJS_INTL_DATETIME_MONTH);
                break;
            case QJS_INTL_DATE_VALUE_PLAIN_MONTH_DAY:
                if (options.date_style)
                    mask = field_bit(QJS_INTL_DATETIME_MONTH) | field_bit(QJS_INTL_DATETIME_DAY);
                break;
            case QJS_INTL_DATE_VALUE_PLAIN_TIME:
                if (options.time_style)
                    mask = time_fields;
                break;
            default:
                mask = date_fields | era_field | time_fields;
                break;
            }
            if (!mask)
                continue;
            /* Preserve the authoritative style and its punctuation whenever
               projection removes no fields. Rebuilding the same skeleton can
               select a different CLDR availableFormat, not the chosen style. */
            if (!(qjs_intl_datetime_component_mask(&components) & ~mask)) {
                error = install_record(formatter, kind, pattern, components);
                if (error)
                    return error;
                continue;
            }
            auto request = components;
            for (unsigned i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
                if (!((mask >> i) & 1))
                    request.values[i] = 0;
            UnicodeString selected;
            QJSIntlDateTimeComponents facts{};

            error = choose_pattern(formatter, generator, request, selected, facts);
            if (!error)
                error = install_record(formatter, kind, selected, facts);
            if (error)
                return error;
        }
        return QJS_INTL_OK;
    }
    error = get_record(formatter, generator, QJS_INTL_DATE_VALUE_DATE, options.required,
                       options.defaults, false);
    if (!error)
        error = get_record(formatter, generator, QJS_INTL_DATE_VALUE_PLAIN_DATE,
                           QJS_INTL_DATE_REQUIRE_DATE, QJS_INTL_DATE_DEFAULT_DATE, true);
    if (!error)
        error =
            get_record(formatter, generator, QJS_INTL_DATE_VALUE_PLAIN_YEAR_MONTH,
                       QJS_INTL_DATE_REQUIRE_YEAR_MONTH, QJS_INTL_DATE_DEFAULT_YEAR_MONTH, true);
    if (!error)
        error = get_record(formatter, generator, QJS_INTL_DATE_VALUE_PLAIN_MONTH_DAY,
                           QJS_INTL_DATE_REQUIRE_MONTH_DAY, QJS_INTL_DATE_DEFAULT_MONTH_DAY, true);
    if (!error)
        error = get_record(formatter, generator, QJS_INTL_DATE_VALUE_PLAIN_TIME,
                           QJS_INTL_DATE_REQUIRE_TIME, QJS_INTL_DATE_DEFAULT_TIME, true);
    if (!error)
        error = get_record(formatter, generator, QJS_INTL_DATE_VALUE_PLAIN_DATE_TIME,
                           QJS_INTL_DATE_REQUIRE_ANY, QJS_INTL_DATE_DEFAULT_ALL, true);
    if (!error)
        error = get_record(
            formatter, generator, QJS_INTL_DATE_VALUE_INSTANT, QJS_INTL_DATE_REQUIRE_ANY,
            options.forced_zone ? QJS_INTL_DATE_DEFAULT_ZONED : QJS_INTL_DATE_DEFAULT_ALL, false);
    return error;
}

QJSIntlPartType field_type(int32_t field)
{
    switch (field) {
    case UDAT_ERA_FIELD:
        return QJS_INTL_PART_ERA;
    case UDAT_YEAR_FIELD:
    case UDAT_EXTENDED_YEAR_FIELD:
        return QJS_INTL_PART_YEAR;
    case UDAT_MONTH_FIELD:
    case UDAT_STANDALONE_MONTH_FIELD:
        return QJS_INTL_PART_MONTH;
    case UDAT_DATE_FIELD:
        return QJS_INTL_PART_DAY;
    case UDAT_HOUR_OF_DAY1_FIELD:
    case UDAT_HOUR_OF_DAY0_FIELD:
    case UDAT_HOUR1_FIELD:
    case UDAT_HOUR0_FIELD:
        return QJS_INTL_PART_HOUR;
    case UDAT_MINUTE_FIELD:
        return QJS_INTL_PART_MINUTE;
    case UDAT_SECOND_FIELD:
        return QJS_INTL_PART_SECOND;
    case UDAT_FRACTIONAL_SECOND_FIELD:
        return QJS_INTL_PART_FRACTIONAL_SECOND;
    case UDAT_DAY_OF_WEEK_FIELD:
    case UDAT_DOW_LOCAL_FIELD:
    case UDAT_STANDALONE_DAY_FIELD:
        return QJS_INTL_PART_WEEKDAY;
    case UDAT_AM_PM_FIELD:
    case UDAT_AM_PM_MIDNIGHT_NOON_FIELD:
    case UDAT_FLEXIBLE_DAY_PERIOD_FIELD:
        return QJS_INTL_PART_DAY_PERIOD;
    case UDAT_TIMEZONE_FIELD:
    case UDAT_TIMEZONE_RFC_FIELD:
    case UDAT_TIMEZONE_GENERIC_FIELD:
    case UDAT_TIMEZONE_SPECIAL_FIELD:
    case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD:
    case UDAT_TIMEZONE_ISO_FIELD:
    case UDAT_TIMEZONE_ISO_LOCAL_FIELD:
        return QJS_INTL_PART_TIME_ZONE_NAME;
    case UDAT_RELATED_YEAR_FIELD:
        return QJS_INTL_PART_RELATED_YEAR;
    case UDAT_YEAR_NAME_FIELD:
        return QJS_INTL_PART_YEAR_NAME;
    default:
        return QJS_INTL_PART_LITERAL;
    }
}

struct FieldMap {
    const QJSIntlAllocator &allocator;
    unsigned char *fields = nullptr, *sources = nullptr;
    size_t length = 0;

    explicit FieldMap(const QJSIntlAllocator &a) : allocator(a) {}
    FieldMap(const FieldMap &) = delete;
    FieldMap &operator=(const FieldMap &) = delete;
    ~FieldMap()
    {
        if (fields)
            allocator.free(allocator.opaque, fields);
        if (sources)
            allocator.free(allocator.opaque, sources);
    }
    QJSIntlStatus open(const UnicodeString &text, size_t limit)
    {
        if (text.isBogus())
            return QJS_INTL_NO_MEMORY;
        length = static_cast<size_t>(text.length());
        if (length > limit)
            return QJS_INTL_OVERFLOW;
        if (!length)
            return QJS_INTL_OK;
        fields = static_cast<unsigned char *>(allocator.malloc(allocator.opaque, length));
        sources = static_cast<unsigned char *>(allocator.malloc(allocator.opaque, length));
        if (!fields || !sources)
            return QJS_INTL_NO_MEMORY;
        std::memset(fields, QJS_INTL_PART_LITERAL, length);
        std::memset(sources, QJS_INTL_RANGE_SHARED, length);
        return QJS_INTL_OK;
    }
    QJSIntlStatus fill(bool origin, int32_t begin, int32_t end, unsigned value)
    {
        if (begin < 0 || end < begin || static_cast<size_t>(end) > length)
            return QJS_INTL_DATA_ERROR;
        if (end > begin)
            std::memset((origin ? sources : fields) + begin, value,
                        static_cast<size_t>(end - begin));
        return QJS_INTL_OK;
    }
};

template <class Part, class Result>
QJSIntlStatus copy_result(const QJSIntlAllocator &allocator, const UnicodeString &text,
                          const FieldMap &map, Result *out)
{
    size_t count = 0, start = 0;

    while (start < map.length) {
        size_t end = start + 1;
        while (end < map.length && map.fields[end] == map.fields[start] &&
               map.sources[end] == map.sources[start])
            end++;
        count++;
        start = end;
    }
    if (count > SIZE_MAX / sizeof(Part) || map.length > SIZE_MAX / sizeof(uint16_t))
        return QJS_INTL_OVERFLOW;
    if (map.length) {
        out->text = static_cast<uint16_t *>(
            allocator.malloc(allocator.opaque, map.length * sizeof(uint16_t)));
        if (!out->text)
            return QJS_INTL_NO_MEMORY;
        std::memcpy(out->text, text.getBuffer(), map.length * sizeof(uint16_t));
    }
    out->length = map.length;
    if (count) {
        out->parts = static_cast<Part *>(allocator.malloc(allocator.opaque, count * sizeof(Part)));
        if (!out->parts) {
            allocator.free(allocator.opaque, out->text);
            *out = {};
            return QJS_INTL_NO_MEMORY;
        }
    }
    start = 0;
    while (start < map.length) {
        size_t end = start + 1;
        while (end < map.length && map.fields[end] == map.fields[start] &&
               map.sources[end] == map.sources[start])
            end++;
        Part part{};
        part.start = start;
        part.end = end;
        part.type = static_cast<QJSIntlPartType>(map.fields[start]);
        if constexpr (std::is_same<Part, QJSIntlRangePart>::value)
            part.source = static_cast<QJSIntlRangeSource>(map.sources[start]);
        out->parts[out->part_count++] = part;
        start = end;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus epoch_millis(int64_t seconds, uint32_t nanos, int64_t *out)
{
    if (nanos >= 1000000000)
        return QJS_INTL_INVALID_ARGUMENT;
    /* Kind-specific Date/Temporal limits belong to the frontend. Valid plain
       values extend beyond TimeClip, including an earlier YearMonth anchor.
       Only reject integer overflow or loss in ICU's binary64 millisecond API. */
    if (seconds < INT64_MIN / 1000 || seconds > (INT64_MAX - 999) / 1000)
        return QJS_INTL_OVERFLOW;
    int64_t millis = seconds * 1000 + nanos / 1000000;
    if (millis < INT64_C(-9007199254740991) || millis > INT64_C(9007199254740991))
        return QJS_INTL_OVERFLOW;
    *out = millis;
    return QJS_INTL_OK;
}

Owner<icu::Calendar> value_calendar(const ICUDateRecord &record, int64_t millis, UErrorCode &status)
{
    Owner<icu::Calendar> calendar(record.single->getCalendar()->clone());

    if (!calendar) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    calendar->setTime(static_cast<UDate>(millis), status);
    return calendar;
}

Owner<icu::Calendar> interval_calendar(const ICUDateRecord &record, int64_t millis,
                                       UErrorCode &status)
{
    Owner<icu::Calendar> calendar(record.range->getDateFormat()->getCalendar()->clone());
    if (!calendar) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    // Interval inputs must share the single formatter's proleptic policy too;
    // their distinct prototype otherwise restores the historical 1582 cutover.
    if (auto *gregorian = dynamic_cast<icu::GregorianCalendar *>(calendar.get()))
        gregorian->setGregorianChange(-DBL_MAX, status);
    calendar->setTime(static_cast<UDate>(millis), status);
    return calendar;
}

/* ICU formats Chinese/Dangi G as a numeric cycle only for its private
   concrete Calendar classes. Keep the ordinary G pattern and its field spans:
   a private per-call symbols clone maps index zero to the ICU-formatted actual
   cycle. This changes neither the instant nor the shared formatter/prototype.
   It is restricted to the uncommon records that actually display an era. */
Owner<icu::SimpleDateFormat> era_formatter(const ICUDateRecord &record,
                                           LunarFormatCalendar &calendar, UErrorCode &status)
{
    int cycle = calendar.get(UCAL_ERA, status);
    Owner<icu::SimpleDateFormat> single(record.single->clone());
    const icu::NumberFormat *original = record.single->getNumberFormatForField('G');
    Owner<icu::NumberFormat> numbers(original ? original->clone() : nullptr);
    if (!single || !numbers) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    numbers->setMinimumIntegerDigits(1);
    numbers->setMaximumIntegerDigits(9);
    UnicodeString label;
    icu::FieldPosition position;
    numbers->format(static_cast<int64_t>(cycle), label, position, status);
    if (U_FAILURE(status))
        return nullptr;
    if (label.isBogus()) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    Owner<icu::DateFormatSymbols> symbols(
        new icu::DateFormatSymbols(*single->getDateFormatSymbols()));
    if (!symbols) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    symbols->setEras(&label, 1);
    symbols->setEraNames(&label, 1);
    symbols->setNarrowEras(&label, 1);
    // These ICU setters have no error status. Verify every retained copy before
    // exposing output, including a foreign allocation that retained old data.
    for (unsigned width = 0; width < 3; width++) {
        int32_t count = 0;
        const UnicodeString *stored = width == 0   ? symbols->getEras(count)
                                      : width == 1 ? symbols->getEraNames(count)
                                                   : symbols->getNarrowEras(count);
        if (!stored || count != 1 || stored[0].isBogus() || stored[0] != label) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
    }
    single->adoptDateFormatSymbols(symbols.release());
    calendar.use_era_symbol_index();
    return single;
}

/* FormatDateTimePattern presents a nonpositive year as 1-year. Override
   only y's number formatting: replacing the Calendar would lose ICU's concrete
   Hebrew month rules and calendar-specific related-year calculations. */
class PositiveYearFormat final : public icu::NumberFormat {
  public:
    explicit PositiveYearFormat(const icu::NumberFormat &source)
        : icu::NumberFormat(source), source_(source.clone())
    {
    }
    PositiveYearFormat(const PositiveYearFormat &other)
        : icu::NumberFormat(other), source_(other.source_ ? other.source_->clone() : nullptr)
    {
    }
    bool valid() const
    {
        return source_ != nullptr;
    }
    PositiveYearFormat *clone() const override
    {
        auto *result = new PositiveYearFormat(*this);
        if (result && !result->valid()) {
            delete result;
            return nullptr;
        }
        return result;
    }
    UClassID getDynamicClassID() const override
    {
        static char identity;
        return &identity;
    }
    using icu::NumberFormat::format;
    UnicodeString &format(double n, UnicodeString &out, icu::FieldPosition &pos) const override
    {
        return source_->format(n <= 0 ? 1 - n : n, out, pos);
    }
    UnicodeString &format(int32_t n, UnicodeString &out, icu::FieldPosition &pos) const override
    {
        return format(static_cast<int64_t>(n), out, pos);
    }
    UnicodeString &format(int64_t n, UnicodeString &out, icu::FieldPosition &pos) const override
    {
        // Calendar years are int32. Keep this general overload free of signed
        // overflow even if a future ICU caller passes an unrelated extreme.
        if (n <= INT64_MIN + 1) {
            out.setToBogus();
            return out;
        }
        return source_->format(n <= 0 ? 1 - n : n, out, pos);
    }
    UnicodeString &format(int64_t n, UnicodeString &out, icu::FieldPosition &pos,
                          UErrorCode &status) const override
    {
        if (U_FAILURE(status))
            return out;
        if (n <= INT64_MIN + 1) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return out;
        }
        return source_->format(n <= 0 ? 1 - n : n, out, pos, status);
    }
    void parse(const UnicodeString &text, icu::Formattable &out,
               icu::ParsePosition &position) const override
    {
        source_->parse(text, out, position);
    }
    void setMinimumIntegerDigits(int32_t digits) override
    {
        icu::NumberFormat::setMinimumIntegerDigits(digits);
        source_->setMinimumIntegerDigits(digits);
    }
    void setMaximumIntegerDigits(int32_t digits) override
    {
        icu::NumberFormat::setMaximumIntegerDigits(digits);
        source_->setMaximumIntegerDigits(digits);
    }

  private:
    Owner<icu::NumberFormat> source_;
};

int semantic_era(icu::Calendar &calendar, UErrorCode &status)
{
    const char *type = calendar.getType();
    if (!std::strncmp(type, "islamic", 7))
        return calendar.get(UCAL_EXTENDED_YEAR, status) <= 0 ? 1 : 0;
    if (!std::strcmp(type, "coptic"))
        return 1;
    return calendar.get(UCAL_ERA, status);
}

bool corrected_presentation(icu::Calendar &calendar, UErrorCode &status)
{
    int year = calendar.get(UCAL_YEAR, status);
    int era = calendar.get(UCAL_ERA, status);
    return year <= 0 || era != semantic_era(calendar, status);
}

Owner<icu::SimpleDateFormat> presentation_formatter(const ICUDateRecord &record,
                                                    icu::Calendar &calendar, UErrorCode &status)
{
    Owner<icu::SimpleDateFormat> single(record.single->clone());
    if (!single) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    if (calendar.get(UCAL_YEAR, status) <= 0) {
        const auto *original = record.single->getNumberFormatForField('y');
        Owner<PositiveYearFormat> numbers(original ? new PositiveYearFormat(*original) : nullptr);
        if (!numbers || !numbers->valid()) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        single->adoptNumberFormat(UnicodeString("y"), numbers.release(), status);
    }
    int era = calendar.get(UCAL_ERA, status);
    int target = semantic_era(calendar, status);
    if (U_FAILURE(status))
        return nullptr;
    if (era != target) {
        // ICU already ships the BH/AM labels but its raw chronology uses the
        // old era convention. Remap existing symbols, never invent era names.
        Owner<icu::DateFormatSymbols> symbols(
            new icu::DateFormatSymbols(*single->getDateFormatSymbols()));
        if (!symbols) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        for (unsigned width = 0; width < 3; width++) {
            int32_t count = 0;
            const UnicodeString *values = width == 0   ? symbols->getEras(count)
                                          : width == 1 ? symbols->getEraNames(count)
                                                       : symbols->getNarrowEras(count);
            if (era != 0 || !values || target < 0 || target >= count || values[target].isEmpty()) {
                status = U_UNSUPPORTED_ERROR;
                return nullptr;
            }
            UnicodeString label(values[target]);
            if (width == 0)
                symbols->setEras(&label, 1);
            else if (width == 1)
                symbols->setEraNames(&label, 1);
            else
                symbols->setNarrowEras(&label, 1);
            values = width == 0   ? symbols->getEras(count)
                     : width == 1 ? symbols->getEraNames(count)
                                  : symbols->getNarrowEras(count);
            if (!values || count != 1 || values[0].isBogus() || values[0] != label) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return nullptr;
            }
        }
        single->adoptDateFormatSymbols(symbols.release());
    }
    return single;
}

QJSIntlStatus single_result(const QJSIntlDateTimeFormatter &formatter, const ICUDateRecord &record,
                            int64_t millis, QJSIntlFormatted *out)
{
    UErrorCode status = U_ZERO_ERROR;
    Owner<icu::Calendar> calendar = value_calendar(record, millis, status);
    UnicodeString text;
    icu::FieldPositionIterator positions;

    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    Owner<icu::SimpleDateFormat> era;
    const icu::SimpleDateFormat *single = record.single;
    if (record.numeric_era) {
        era = era_formatter(record, static_cast<LunarFormatCalendar &>(*calendar), status);
        if (U_FAILURE(status))
            return qjs_icu4c_status(status);
        single = era.get();
    } else if (corrected_presentation(*calendar, status)) {
        era = presentation_formatter(record, *calendar, status);
        if (U_FAILURE(status))
            return qjs_icu4c_status(status);
        single = era.get();
    }
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    single->format(*calendar, text, &positions, status);
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    FieldMap map(formatter.allocator);
    QJSIntlStatus error = map.open(text, formatter.options.maximum_output_length);
    icu::FieldPosition position;

    while (!error && positions.next(position))
        error = map.fill(false, position.getBeginIndex(), position.getEndIndex(),
                         field_type(position.getField()));
    return error ? error : copy_result<QJSIntlPart>(formatter.allocator, text, map, out);
}

QJSIntlStatus validate_options(const QJSIntlDateTimeOptions *options)
{
    int score;

    if (!options || !options->maximum_output_length ||
        options->date_style > QJS_INTL_DATE_STYLE_WIDTH_COUNT ||
        options->time_style > QJS_INTL_DATE_STYLE_WIDTH_COUNT ||
        static_cast<unsigned>(options->hour_cycle) > QJS_INTL_DATE_H24 ||
        static_cast<unsigned>(options->required) > QJS_INTL_DATE_REQUIRE_MONTH_DAY ||
        static_cast<unsigned>(options->defaults) > QJS_INTL_DATE_DEFAULT_ZONED ||
        options->best_fit > 1 || options->forced_zone > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    QJSIntlStatus error =
        qjs_intl_datetime_basic_score(&options->components, &options->components, &score);

    if (error)
        return error;
    if ((options->date_style || options->time_style) &&
        qjs_intl_datetime_component_mask(&options->components))
        return QJS_INTL_INVALID_ARGUMENT;
    if ((options->required == QJS_INTL_DATE_REQUIRE_DATE && options->time_style) ||
        (options->required == QJS_INTL_DATE_REQUIRE_TIME && options->date_style))
        return QJS_INTL_INVALID_ARGUMENT;
    return QJS_INTL_OK;
}
} // namespace

extern "C" void qjs_intl_datetime_formatter_close(QJSIntlDateTimeFormatter *formatter)
{
    if (formatter) {
        const QJSIntlAllocator allocator = formatter->allocator;

        for (auto &record : formatter->records) {
            delete record.single;
            delete record.range;
        }
        if (formatter->locale)
            allocator.free(allocator.opaque, formatter->locale);
        if (formatter->fallback_storage)
            allocator.free(allocator.opaque, formatter->fallback_storage);
        allocator.free(allocator.opaque, formatter);
    }
}

extern "C" QJSIntlStatus qjs_icu4c_datetime_create(const QJSIntlAllocator *allocator,
                                                   QJSIntlBytes locale, QJSIntlBytes calendar,
                                                   QJSIntlBytes system,
                                                   const QJSTimeZoneIdentifier *zone,
                                                   const QJSIntlDateTimeOptions *options,
                                                   QJSIntlDateTimeFormatter **out)
{
    QJSIntlDateTimeFormatter *formatter = nullptr;
    QJSIntlStatus error;
    char *base = nullptr, *selected = nullptr;
    const char *const cycles[] = {"h11", "h12", "h23", "h24"};

    if (out)
        *out = nullptr;
    if (!out || !allocator || !allocator->malloc || !allocator->free || !zone ||
        calendar.length >= sizeof(formatter->calendar) ||
        system.length >= sizeof(formatter->system))
        return QJS_INTL_INVALID_ARGUMENT;
    error = validate_options(options);
    if (error)
        return error;
    try {
        formatter = static_cast<QJSIntlDateTimeFormatter *>(
            allocator->malloc(allocator->opaque, sizeof(*formatter)));
        if (!formatter)
            return QJS_INTL_NO_MEMORY;
        std::memset(formatter, 0, sizeof(*formatter));
        formatter->allocator = *allocator;
        formatter->options = *options;
        std::memcpy(formatter->calendar, calendar.data, calendar.length);
        std::memcpy(formatter->system, system.data, system.length);
        std::memcpy(formatter->zone, zone->identifier, sizeof(formatter->zone));
        error = qjs_icu4c_locale_id(allocator, locale, &base);
        const char *const keys[] = {"ca", "nu", "hc"};
        QJSIntlBytes values[] = {calendar, system, {cycles[options->hour_cycle], 3}};

        for (unsigned i = 0; !error && i < 3; i++) {
            error = qjs_icu4c_locale_keyword(allocator, base, keys[i], values[i], &selected);
            allocator->free(allocator->opaque, base);
            base = selected;
            selected = nullptr;
        }
        if (!error) {
            formatter->locale = base;
            base = nullptr;
            UErrorCode status = U_ZERO_ERROR;
            Owner<icu::DateTimePatternGenerator> generator(
                icu::DateTimePatternGenerator::createInstance(icu::Locale(formatter->locale),
                                                              status));

            error = qjs_icu4c_status(status);
            if (!error && !generator)
                error = QJS_INTL_NO_MEMORY;
            if (!error)
                error = select_records(*formatter, *generator);
            if (!error) {
                icu::DateIntervalInfo intervals(icu::Locale(formatter->locale), status);
                UnicodeString fallback;

                intervals.getFallbackIntervalPattern(fallback);
                error = qjs_icu4c_status(status);
                if (!error)
                    error = utf8(*allocator, fallback, &formatter->fallback_storage);
                if (!error)
                    error = qjs_intl_pair_pattern_parse(
                        {formatter->fallback_storage, std::strlen(formatter->fallback_storage)},
                        &formatter->fallback);
            }
        }
    } catch (const std::bad_alloc &) {
        error = QJS_INTL_NO_MEMORY;
    } catch (...) {
        error = QJS_INTL_DATA_ERROR;
    }
    if (base)
        allocator->free(allocator->opaque, base);
    if (!error) {
        *out = formatter;
        return QJS_INTL_OK;
    }
    qjs_intl_datetime_formatter_close(formatter);
    return error;
}

extern "C" QJSIntlStatus qjs_intl_datetime_formatter_info(const QJSIntlDateTimeFormatter *formatter,
                                                          QJSIntlBytes *calendar,
                                                          QJSIntlBytes *system, const char **zone,
                                                          QJSIntlDateTimeComponents *components)
{
    if (calendar)
        *calendar = {};
    if (system)
        *system = {};
    if (zone)
        *zone = nullptr;
    if (components)
        *components = {};
    if (!formatter || !calendar || !system || !zone || !components)
        return QJS_INTL_INVALID_ARGUMENT;
    *calendar = {formatter->calendar, std::strlen(formatter->calendar)};
    *system = {formatter->system, std::strlen(formatter->system)};
    *zone = formatter->zone;
    *components = formatter->records[QJS_INTL_DATE_VALUE_DATE].components;
    return QJS_INTL_OK;
}

extern "C" QJSIntlStatus
qjs_intl_datetime_check_value_format(const QJSIntlDateTimeFormatter *formatter,
                                     QJSIntlDateValueKind kind)
{
    if (!formatter || static_cast<unsigned>(kind) >= QJS_INTL_DATE_VALUE_KIND_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    return formatter->records[kind].single ? QJS_INTL_OK : QJS_INTL_UNSUPPORTED;
}

extern "C" QJSIntlStatus qjs_intl_datetime_format(const QJSIntlDateTimeFormatter *formatter,
                                                  QJSIntlDateValueKind kind, int64_t seconds,
                                                  uint32_t nanos, QJSIntlFormatted *out)
{
    int64_t millis;

    if (out)
        *out = {};
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    QJSIntlStatus error = qjs_intl_datetime_check_value_format(formatter, kind);

    if (!error)
        error = epoch_millis(seconds, nanos, &millis);
    if (!error) {
        try {
            error = single_result(*formatter, formatter->records[kind], millis, out);
        } catch (const std::bad_alloc &) {
            error = QJS_INTL_NO_MEMORY;
        } catch (...) {
            error = QJS_INTL_DATA_ERROR;
        }
    }
    if (error && formatter)
        qjs_intl_formatted_clear(&formatter->allocator, out);
    return error;
}

extern "C" QJSIntlStatus
qjs_intl_datetime_formatter_prepare_ranges(QJSIntlDateTimeFormatter *formatter)
{
    if (!formatter)
        return QJS_INTL_INVALID_ARGUMENT;
    if (formatter->ranges_prepared)
        return QJS_INTL_OK;
    try {
        Owner<icu::DateIntervalFormat> prepared[QJS_INTL_DATE_VALUE_KIND_COUNT];
        UErrorCode status = U_ZERO_ERROR;

        for (unsigned kind = 0; kind < QJS_INTL_DATE_VALUE_KIND_COUNT; kind++) {
            const auto &record = formatter->records[kind];
            if (!record.single)
                continue;
            UnicodeString pattern;
            record.single->toPattern(pattern);
            UnicodeString key = icu::DateTimePatternGenerator::staticGetSkeleton(pattern, status);

            prepared[kind].reset(icu::DateIntervalFormat::createInstance(
                key, icu::Locale(formatter->locale), status));
            if (U_FAILURE(status))
                return qjs_icu4c_status(status);
            if (!prepared[kind])
                return QJS_INTL_NO_MEMORY;
            prepared[kind]->setTimeZone(record.single->getTimeZone());
        }
        // Publish only after every optional record is ready; failure is retryable.
        for (unsigned kind = 0; kind < QJS_INTL_DATE_VALUE_KIND_COUNT; kind++)
            formatter->records[kind].range = prepared[kind].release();
        formatter->ranges_prepared = 1;
        return QJS_INTL_OK;
    } catch (const std::bad_alloc &) {
        return QJS_INTL_NO_MEMORY;
    } catch (...) {
        return QJS_INTL_DATA_ERROR;
    }
}

namespace {
int64_t displayed_millis(const ICUDateRecord &record, int64_t millis)
{
    int fraction = record.components.values[QJS_INTL_DATETIME_FRACTION];
    int64_t quantum = fraction == 3 ? 1 : fraction == 2 ? 10 : fraction == 1 ? 100 : 1000;
    int64_t remainder = millis % quantum;

    // Truncation would move a negative instant into the next displayed second.
    return millis - (remainder < 0 ? remainder + quantum : remainder);
}

bool part_matches(const QJSIntlFormatted &single, QJSIntlPartType type, const UnicodeString &text,
                  int32_t start, int32_t end)
{
    for (size_t i = 0; i < single.part_count; i++) {
        const auto &part = single.parts[i];
        if (part.type == type && part.end - part.start == static_cast<size_t>(end - start) &&
            !std::memcmp(single.text + part.start, text.getBuffer() + start,
                         static_cast<size_t>(end - start) * sizeof(uint16_t)))
            return true;
    }
    return false;
}

QJSIntlStatus fallback_range(const QJSIntlDateTimeFormatter &formatter, const QJSIntlFormatted &a,
                             const QJSIntlFormatted &b, QJSIntlFormattedRange *out)
{
    const auto &layout = formatter.fallback;
    const QJSIntlFormatted *bodies[] = {&a, &b};
    QJSIntlRangeSource sources[] = {QJS_INTL_RANGE_START, QJS_INTL_RANGE_END};
    QJSIntlRangePiece pieces[] = {
        {nullptr, layout.prefix, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED},
        {bodies[layout.first], {}, QJS_INTL_PART_LITERAL, sources[layout.first]},
        {nullptr, layout.separator, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED},
        {bodies[1 - layout.first], {}, QJS_INTL_PART_LITERAL, sources[1 - layout.first]},
        {nullptr, layout.suffix, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED},
    };
    return qjs_intl_range_compose(&formatter.allocator, pieces, sizeof(pieces) / sizeof(*pieces),
                                  formatter.options.maximum_output_length, out);
}

bool same_period(const QJSIntlFormatted &a, const QJSIntlFormatted &b)
{
    const QJSIntlPart *pa = nullptr, *pb = nullptr;
    for (size_t i = 0; i < a.part_count; i++)
        if (a.parts[i].type == QJS_INTL_PART_DAY_PERIOD)
            pa = &a.parts[i];
    for (size_t i = 0; i < b.part_count; i++)
        if (b.parts[i].type == QJS_INTL_PART_DAY_PERIOD)
            pb = &b.parts[i];
    if (!pa || !pb)
        return pa == pb;
    return pa->end - pa->start == pb->end - pb->start &&
           !std::memcmp(a.text + pa->start, b.text + pb->start,
                        (pa->end - pa->start) * sizeof(uint16_t));
}

/* Complete-endpoint fallback still collapses equal relevant fields. Compare
   the coarser prefix through the finest displayed field, including omitted
   coarser fields: January in two different years is not an equal month range.
   Month identity includes the leap flag, never the possibly ambiguous name.
   ECMA-402 permits an ILD string specifically for the DayPeriod comparison. */
bool same_range_fields(const ICUDateRecord &record, icu::Calendar &a, icu::Calendar &b,
                       const QJSIntlFormatted &text_a, const QJSIntlFormatted &text_b,
                       UErrorCode &status)
{
    static const int levels[QJS_INTL_DATETIME_COMPONENT_COUNT] = {
        3, 0, 1, 2, 3, 5, 6, 7, 8, 9, -1,
    };
    static const UCalendarDateFields fields[] = {
        UCAL_ERA,   UCAL_EXTENDED_YEAR, UCAL_MONTH,  UCAL_DATE,   UCAL_AM_PM,
        UCAL_AM_PM, UCAL_HOUR_OF_DAY,   UCAL_MINUTE, UCAL_SECOND, UCAL_MILLISECOND,
    };
    int finest = -1;
    for (unsigned i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if (record.components.values[i] && levels[i] > finest)
            finest = levels[i];
    if (finest < 0)
        finest = 9;
    for (int i = 0; i <= finest; i++) {
        if (i == 5) {
            if (!same_period(text_a, text_b))
                return false;
            continue;
        }
        int first = i == 0 ? semantic_era(a, status) : a.get(fields[i], status);
        int last = i == 0 ? semantic_era(b, status) : b.get(fields[i], status);
        if (i == 9) {
            int precision = record.components.values[QJS_INTL_DATETIME_FRACTION];
            int divisor = precision == 1 ? 100 : precision == 2 ? 10 : 1;
            first /= divisor;
            last /= divisor;
        }
        if (U_FAILURE(status) || first != last)
            return false;
        if (i == 2 && a.get(UCAL_IS_LEAP_MONTH, status) != b.get(UCAL_IS_LEAP_MONTH, status))
            return false;
    }
    return U_SUCCESS(status);
}

bool same_chronology(icu::Calendar &a, icu::Calendar &b, UErrorCode &status)
{
    static const UCalendarDateFields fields[] = {
        UCAL_ERA, UCAL_YEAR, UCAL_MONTH, UCAL_IS_LEAP_MONTH, UCAL_DATE,
    };
    for (auto field : fields)
        if (a.get(field, status) != b.get(field, status) || U_FAILURE(status))
            return false;
    return true;
}

QJSIntlStatus interval_result(const QJSIntlDateTimeFormatter &formatter,
                              const ICUDateRecord &record, int64_t first, int64_t last,
                              QJSIntlFormattedRange *out)
{
    QJSIntlFormatted a{}, b{};
    QJSIntlStatus error = single_result(formatter, record, first, &a);

    if (!error)
        error = single_result(formatter, record, last, &b);
    if (error) {
        qjs_intl_formatted_clear(&formatter.allocator, &a);
        qjs_intl_formatted_clear(&formatter.allocator, &b);
        return error;
    }
    try {
        UErrorCode status = U_ZERO_ERROR;
        bool projected = dynamic_cast<const LunarFormatCalendar *>(record.single->getCalendar());
        bool equal = displayed_millis(record, first) == displayed_millis(record, last);
        bool raw_compatible = true;
        if (!equal) {
            auto ca = value_calendar(record, first, status);
            auto cb = value_calendar(record, last, status);
            if (U_SUCCESS(status))
                equal = same_range_fields(record, *ca, *cb, a, b, status);
            if (U_SUCCESS(status))
                raw_compatible =
                    !corrected_presentation(*ca, status) && !corrected_presentation(*cb, status);
            if (projected && !equal && raw_compatible && U_SUCCESS(status) && record.range) {
                auto ra = interval_calendar(record, first, status);
                auto rb = interval_calendar(record, last, status);
                if (U_SUCCESS(status)) {
                    raw_compatible =
                        same_chronology(*ca, *ra, status) && same_chronology(*cb, *rb, status);
                    // ICU can reject a legal distant date covered by the
                    // explicitly approved shared approximation. Preserve other
                    // resource/allocation errors instead of disguising them.
                    if (status == U_ILLEGAL_ARGUMENT_ERROR) {
                        status = U_ZERO_ERROR;
                        raw_compatible = false;
                    }
                }
            }
        }
        if (U_FAILURE(status)) {
            error = qjs_icu4c_status(status);
        } else if (equal) {
            QJSIntlRangePiece shared{&a, {}, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED};

            error = qjs_intl_range_compose(&formatter.allocator, &shared, 1,
                                           formatter.options.maximum_output_length, out);
        } else if (!record.range || !raw_compatible) {
            error = fallback_range(formatter, a, b, out);
        } else {
            UErrorCode status = U_ZERO_ERROR;
            auto first_calendar =
                interval_calendar(record, displayed_millis(record, first), status);
            auto last_calendar = interval_calendar(record, displayed_millis(record, last), status);

            if (U_FAILURE(status)) {
                error = qjs_icu4c_status(status);
            } else {
                auto value = record.range->formatToValue(*first_calendar, *last_calendar, status);
                UnicodeString text = value.toString(status);
                FieldMap map(formatter.allocator);

                error = qjs_icu4c_status(status);
                if (!error)
                    error = map.open(text, formatter.options.maximum_output_length);
                icu::ConstrainedFieldPosition position;
                while (!error && value.nextPosition(position, status)) {
                    if (position.getCategory() == UFIELD_CATEGORY_DATE)
                        error = map.fill(false, position.getStart(), position.getLimit(),
                                         field_type(position.getField()));
                    else if (position.getCategory() == UFIELD_CATEGORY_DATE_INTERVAL_SPAN) {
                        if (position.getField() < 0 || position.getField() > 1)
                            error = QJS_INTL_DATA_ERROR;
                        else
                            error = map.fill(true, position.getStart(), position.getLimit(),
                                             position.getField() ? QJS_INTL_RANGE_END
                                                                 : QJS_INTL_RANGE_START);
                    }
                }
                if (!error)
                    error = qjs_icu4c_status(status);
                bool compatible = true;
                bool has_start = false, has_end = false;
                for (size_t at = 0; !error && at < map.length;) {
                    size_t end = at + 1;
                    while (end < map.length && map.fields[end] == map.fields[at] &&
                           map.sources[end] == map.sources[at])
                        end++;
                    has_start |= map.sources[at] == QJS_INTL_RANGE_START;
                    has_end |= map.sources[at] == QJS_INTL_RANGE_END;
                    auto type = static_cast<QJSIntlPartType>(map.fields[at]);
                    if (type != QJS_INTL_PART_LITERAL) {
                        unsigned origin = map.sources[at];
                        if (origin != QJS_INTL_RANGE_END && !part_matches(a, type, text, at, end))
                            compatible = false;
                        if (origin != QJS_INTL_RANGE_START && !part_matches(b, type, text, at, end))
                            compatible = false;
                    }
                    at = end;
                }
                // ICU's largest-difference selection can ignore leap-month
                // identity. A semantically distinct projected range must not
                // become a shared-only result just because its labels collide.
                if (!has_start || !has_end)
                    compatible = false;
                /* An ICU interval skeleton may silently drop selected padding
                   or change a name width. Keep the selected single-value record
                   authoritative and use the locale's actual fallback template. */
                if (!error)
                    error = compatible
                                ? copy_result<QJSIntlRangePart>(formatter.allocator, text, map, out)
                                : fallback_range(formatter, a, b, out);
            }
        }
    } catch (...) {
        qjs_intl_formatted_clear(&formatter.allocator, &a);
        qjs_intl_formatted_clear(&formatter.allocator, &b);
        throw;
    }
    qjs_intl_formatted_clear(&formatter.allocator, &a);
    qjs_intl_formatted_clear(&formatter.allocator, &b);
    return error;
}
} // namespace

extern "C" QJSIntlStatus qjs_intl_datetime_format_range(const QJSIntlDateTimeFormatter *formatter,
                                                        QJSIntlDateValueKind kind,
                                                        int64_t first_seconds, uint32_t first_nanos,
                                                        int64_t last_seconds, uint32_t last_nanos,
                                                        QJSIntlFormattedRange *out)
{
    int64_t first, last;

    if (out)
        *out = {};
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    QJSIntlStatus error = qjs_intl_datetime_check_value_format(formatter, kind);

    if (!error)
        error = epoch_millis(first_seconds, first_nanos, &first);
    if (!error)
        error = epoch_millis(last_seconds, last_nanos, &last);
    if (!error) {
        try {
            error = interval_result(*formatter, formatter->records[kind], first, last, out);
        } catch (const std::bad_alloc &) {
            error = QJS_INTL_NO_MEMORY;
        } catch (...) {
            error = QJS_INTL_DATA_ERROR;
        }
    }
    if (error && formatter)
        qjs_intl_formatted_range_clear(&formatter->allocator, out);
    return error;
}
