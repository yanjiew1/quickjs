/* Private ICU4X calendar name payloads.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
use crate::pattern::{SinglePattern, Text, single_pattern};
use crate::{BACKEND, RANGE, UNSUPPORTED, write_result};
use icu_datetime::provider::names::*;
use icu_datetime::provider::semantic_skeletons::marker_attrs::{self, Context, Length};
use icu_locale_core::Locale;
use icu_pattern::SinglePlaceholderPattern;
use icu_provider::{
    DataErrorKind, DataIdentifierBorrowed, DataMarker, DataMarkerAttributes, DataPayload,
    DataProvider, DataRequest, marker::DataMarkerExt,
};

mod source {
    extern crate alloc;
    use icu_datetime_data::*;
    pub mod icu {
        pub use icu_datetime as datetime;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_datetime_names_year_buddhist_v1!(Provider);
    impl_datetime_names_year_chinese_v1!(Provider);
    impl_datetime_names_year_coptic_v1!(Provider);
    impl_datetime_names_year_dangi_v1!(Provider);
    impl_datetime_names_year_ethiopian_v1!(Provider);
    impl_datetime_names_year_gregorian_v1!(Provider);
    impl_datetime_names_year_hebrew_v1!(Provider);
    impl_datetime_names_year_indian_v1!(Provider);
    impl_datetime_names_year_hijri_v1!(Provider);
    impl_datetime_names_year_japanese_v1!(Provider);
    impl_datetime_names_year_persian_v1!(Provider);
    impl_datetime_names_year_roc_v1!(Provider);
    impl_datetime_names_month_buddhist_v1!(Provider);
    impl_datetime_names_month_chinese_v1!(Provider);
    impl_datetime_names_month_coptic_v1!(Provider);
    impl_datetime_names_month_dangi_v1!(Provider);
    impl_datetime_names_month_ethiopian_v1!(Provider);
    impl_datetime_names_month_gregorian_v1!(Provider);
    impl_datetime_names_month_hebrew_v1!(Provider);
    impl_datetime_names_month_indian_v1!(Provider);
    impl_datetime_names_month_hijri_v1!(Provider);
    impl_datetime_names_month_japanese_v1!(Provider);
    impl_datetime_names_month_persian_v1!(Provider);
    impl_datetime_names_month_roc_v1!(Provider);
    impl_datetime_names_weekday_v1!(Provider);
    impl_datetime_names_dayperiod_v1!(Provider);
}

enum Names {
    Year(DataPayload<DatetimeNamesYearGregorianV1>),
    Month(DataPayload<DatetimeNamesMonthGregorianV1>),
    Weekday(DataPayload<DatetimeNamesWeekdayV1>),
    Period(DataPayload<DatetimeNamesDayperiodV1>),
}
pub struct Handle(Names);
#[repr(C)]
pub struct Meta {
    kind: u32,
    count: usize,
}
#[repr(C)]
pub struct MonthPattern {
    pattern: SinglePattern,
    offset: i32,
}

fn load<M: DataMarker>(
    locale: &Locale,
    attributes: &DataMarkerAttributes,
) -> Result<DataPayload<M>, i32>
where
    source::Provider: DataProvider<M>,
{
    let source_locale = M::make_locale(locale.into());
    let mut request = DataRequest {
        id: DataIdentifierBorrowed::for_marker_attributes_and_locale(attributes, &source_locale),
        ..Default::default()
    };
    request.metadata.silent = true;
    match <source::Provider as DataProvider<M>>::load(&source::Provider, request) {
        Ok(response)
            if response
                .metadata
                .locale
                .as_ref()
                .is_none_or(|v| v == &source_locale) =>
        {
            Ok(response.payload)
        }
        Ok(_) => Err(UNSUPPORTED),
        Err(e) if e.kind == DataErrorKind::IdentifierNotFound => Err(UNSUPPORTED),
        Err(_) => Err(BACKEND),
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_datetime_names_open(
    source: *const u8,
    length: usize,
    calendar: u32,
    family: u32,
    width: u32,
    standalone: u32,
    out: *mut *mut Handle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if source.is_null()
                || length == 0
                || length > isize::MAX as usize
                || calendar >= 16
                || family > 3
                || standalone > 1
                || width == 0
                || width > 6
            {
                return Err(RANGE);
            }
            let source = std::slice::from_raw_parts(source, length);
            let source = if source == b"root" { b"und" } else { source };
            let locale = Locale::try_from_utf8(source).map_err(|_| RANGE)?;
            if standalone == 1 && (family == 0 || family == 3) {
                return Err(UNSUPPORTED);
            }
            let context = if standalone == 1 {
                Context::Standalone
            } else {
                Context::Format
            };
            // ICU4X marker lengths are semantic: wide=5, narrow=4. These are
            // not the same numbers as the incoming LDML field widths.
            let length = match width {
                1 | 2 if family == 1 => Length::Numeric,
                1..=3 => Length::Abbr,
                4 => Length::Wide,
                5 => Length::Narrow,
                6 if family == 2 => Length::Short,
                _ => return Err(RANGE),
            };
            let attributes = marker_attrs::name_attr_for(context, length);
            let names = match family {
                0 => match calendar {
                    0 | 7 => Names::Year(
                        load::<DatetimeNamesYearGregorianV1>(&locale, attributes)?.cast(),
                    ),
                    1 => Names::Year(
                        load::<DatetimeNamesYearBuddhistV1>(&locale, attributes)?.cast(),
                    ),
                    2 => {
                        Names::Year(load::<DatetimeNamesYearChineseV1>(&locale, attributes)?.cast())
                    }
                    3 => {
                        Names::Year(load::<DatetimeNamesYearCopticV1>(&locale, attributes)?.cast())
                    }
                    4 => Names::Year(load::<DatetimeNamesYearDangiV1>(&locale, attributes)?.cast()),
                    5 | 6 => Names::Year(
                        load::<DatetimeNamesYearEthiopianV1>(&locale, attributes)?.cast(),
                    ),
                    8 => {
                        Names::Year(load::<DatetimeNamesYearHebrewV1>(&locale, attributes)?.cast())
                    }
                    9 => {
                        Names::Year(load::<DatetimeNamesYearIndianV1>(&locale, attributes)?.cast())
                    }
                    10..=12 => {
                        Names::Year(load::<DatetimeNamesYearHijriV1>(&locale, attributes)?.cast())
                    }
                    13 => Names::Year(
                        load::<DatetimeNamesYearJapaneseV1>(&locale, attributes)?.cast(),
                    ),
                    14 => {
                        Names::Year(load::<DatetimeNamesYearPersianV1>(&locale, attributes)?.cast())
                    }
                    15 => Names::Year(load::<DatetimeNamesYearRocV1>(&locale, attributes)?.cast()),
                    _ => return Err(RANGE),
                },
                1 => match calendar {
                    0 | 7 => Names::Month(
                        load::<DatetimeNamesMonthGregorianV1>(&locale, attributes)?.cast(),
                    ),
                    1 => Names::Month(
                        load::<DatetimeNamesMonthBuddhistV1>(&locale, attributes)?.cast(),
                    ),
                    2 => Names::Month(
                        load::<DatetimeNamesMonthChineseV1>(&locale, attributes)?.cast(),
                    ),
                    3 => Names::Month(
                        load::<DatetimeNamesMonthCopticV1>(&locale, attributes)?.cast(),
                    ),
                    4 => {
                        Names::Month(load::<DatetimeNamesMonthDangiV1>(&locale, attributes)?.cast())
                    }
                    5 | 6 => Names::Month(
                        load::<DatetimeNamesMonthEthiopianV1>(&locale, attributes)?.cast(),
                    ),
                    8 => Names::Month(
                        load::<DatetimeNamesMonthHebrewV1>(&locale, attributes)?.cast(),
                    ),
                    9 => Names::Month(
                        load::<DatetimeNamesMonthIndianV1>(&locale, attributes)?.cast(),
                    ),
                    10..=12 => {
                        Names::Month(load::<DatetimeNamesMonthHijriV1>(&locale, attributes)?.cast())
                    }
                    13 => Names::Month(
                        load::<DatetimeNamesMonthJapaneseV1>(&locale, attributes)?.cast(),
                    ),
                    14 => Names::Month(
                        load::<DatetimeNamesMonthPersianV1>(&locale, attributes)?.cast(),
                    ),
                    15 => {
                        Names::Month(load::<DatetimeNamesMonthRocV1>(&locale, attributes)?.cast())
                    }
                    _ => return Err(RANGE),
                },
                2 => Names::Weekday(load::<DatetimeNamesWeekdayV1>(&locale, attributes)?),
                3 => Names::Period(load::<DatetimeNamesDayperiodV1>(&locale, attributes)?),
                _ => return Err(RANGE),
            };
            Ok(Box::into_raw(Box::new(Handle(names))))
        })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_datetime_names_close(handle: *mut Handle) {
    if !handle.is_null() {
        unsafe { drop(Box::from_raw(handle)) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_datetime_names_meta(
    handle: *const Handle,
    out: *mut Meta,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            let (kind, count) = match &handle.0 {
                Names::Year(p) => match p.get() {
                    YearNames::FixedEras(v) => (0, v.len()),
                    YearNames::Cyclic(v) => (1, v.len()),
                },
                Names::Month(p) => match p.get() {
                    MonthNames::Linear(v) => (2, v.len()),
                    MonthNames::LeapPattern(v) => (3, v.len().checked_sub(2).ok_or(BACKEND)?),
                    MonthNames::LeapNumeric(_) => (4, 0),
                    MonthNames::LeapNumericWithBase(_) => (5, 0),
                    MonthNames::Numeric => (6, 0),
                },
                Names::Weekday(p) => (7, p.get().names.len()),
                // Never expose the encoded flexible-rule header as label text.
                // Canonical flexible IDs and minute-precise rules stay in C.
                Names::Period(p) => (8, p.get().names.len().min(4)),
            };
            Ok(Meta { kind, count })
        })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_datetime_names_text(
    handle: *const Handle,
    index: usize,
    out: *mut Text,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            let value = match &handle.0 {
                Names::Year(p) => match p.get() {
                    YearNames::FixedEras(v) | YearNames::Cyclic(v) => v.get(index),
                },
                Names::Month(p) => match p.get() {
                    MonthNames::Linear(v) => v.get(index),
                    MonthNames::LeapPattern(v) if index < v.len().saturating_sub(2) => v.get(index),
                    _ => return Err(UNSUPPORTED),
                },
                Names::Weekday(p) => p.get().names.get(index),
                Names::Period(p) if index < 4 => p.get().names.get(index),
                _ => return Err(RANGE),
            }
            .ok_or(RANGE)?;
            Ok(value.into())
        })
    }
}

fn passthrough() -> SinglePattern {
    SinglePattern {
        prefix: "".into(),
        suffix: "".into(),
        has_number: 1,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_datetime_month_pattern(
    handle: *const Handle,
    phase: u32,
    out: *mut MonthPattern,
) -> i32 {
    unsafe {
        write_result(out, || {
            if phase > 2 {
                return Err(RANGE);
            }
            let handle = handle.as_ref().ok_or(RANGE)?;
            let Names::Month(payload) = &handle.0 else {
                return Err(RANGE);
            };
            if phase == 0 {
                return Ok(MonthPattern {
                    pattern: passthrough(),
                    offset: 0,
                });
            }
            let result = match payload.get() {
                MonthNames::LeapPattern(v) => {
                    let count = v.len().checked_sub(2).ok_or(BACKEND)?;
                    let store = v.get(count + (phase == 2) as usize).ok_or(BACKEND)?;
                    let pattern =
                        SinglePlaceholderPattern::from_ref_store(store).map_err(|_| BACKEND)?;
                    MonthPattern {
                        pattern: single_pattern(pattern)?,
                        offset: 0,
                    }
                }
                MonthNames::LeapNumeric(p) if phase == 1 => MonthPattern {
                    pattern: single_pattern(p)?,
                    offset: 0,
                },
                MonthNames::LeapNumeric(_) => MonthPattern {
                    pattern: passthrough(),
                    offset: 0,
                },
                MonthNames::LeapNumericWithBase(v) => {
                    let tuple = v.get((phase == 2) as usize).ok_or(UNSUPPORTED)?;
                    MonthPattern {
                        pattern: single_pattern(&tuple.variable)?,
                        offset: tuple.sized.into(),
                    }
                }
                _ => return Err(UNSUPPORTED),
            };
            Ok(result)
        })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_datetime_check_version(data: *const u8, length: usize) -> i32 {
    if data.is_null() || length > isize::MAX as usize {
        return RANGE;
    }
    match unsafe { std::slice::from_raw_parts(data, length) } {
        v if v == icu_datetime_data::cldr_tag!().as_bytes() => 0,
        _ => BACKEND,
    }
}

// Calendar data owns canonical-era indexes. In particular, future Japanese
// eras must follow the selected public payload instead of a frozen name list.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_datetime_era_index(
    calendar: u32,
    code: *const u8,
    length: usize,
    out: *mut usize,
) -> i32 {
    unsafe {
        write_result(out, || {
            if code.is_null() || length == 0 || length > isize::MAX as usize || calendar >= 16 {
                return Err(RANGE);
            }
            let code = std::slice::from_raw_parts(code, length);
            let index = match (calendar, code) {
                (0 | 7 | 13, b"bce") => 0,
                (0 | 7 | 13, b"ce") => 1,
                (1, b"be")
                | (3 | 8, b"am")
                | (5 | 6, b"aa")
                | (9, b"shaka")
                | (10..=12, b"ah")
                | (14, b"ap")
                | (15, b"broc") => 0,
                (5 | 6, b"am") | (10..=12, b"bh") | (15, b"roc") => 1,
                (13, _) => {
                    use icu_calendar::provider::{Baked, CalendarJapaneseModernV1};
                    let response = <Baked as DataProvider<CalendarJapaneseModernV1>>::load(
                        &Baked,
                        Default::default(),
                    )
                    .map_err(|_| BACKEND)?;
                    response
                        .payload
                        .get()
                        .eras()
                        .find(|(_, name, _)| name.as_bytes() == code)
                        .map(|(_, _, index)| usize::from(index))
                        .ok_or(UNSUPPORTED)?
                }
                _ => return Err(UNSUPPORTED),
            };
            Ok(index)
        })
    }
}
