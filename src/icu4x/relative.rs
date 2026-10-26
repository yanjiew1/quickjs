/* Private ICU4X relative-time pattern metadata.
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
use crate::plural_pattern::{Text, selector};
use crate::{BACKEND, RANGE, UNSUPPORTED, write_result};
use icu_experimental::relativetime::provider::*;
use icu_locale_core::Locale;
use icu_pattern::PatternItem;
use icu_provider::{
    DataIdentifierBorrowed, DataPayload, DataProvider, DataRequest, marker::DataMarkerExt,
};
use source::Provider as Baked;

mod source {
    extern crate alloc;
    use icu_experimental_data::*;
    pub mod icu {
        pub use icu_experimental as experimental;
        pub use icu_plurals as plurals;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_datetime_relative_second_long_v1!(Provider);
    impl_datetime_relative_second_short_v1!(Provider);
    impl_datetime_relative_second_narrow_v1!(Provider);
    impl_datetime_relative_minute_long_v1!(Provider);
    impl_datetime_relative_minute_short_v1!(Provider);
    impl_datetime_relative_minute_narrow_v1!(Provider);
    impl_datetime_relative_hour_long_v1!(Provider);
    impl_datetime_relative_hour_short_v1!(Provider);
    impl_datetime_relative_hour_narrow_v1!(Provider);
    impl_datetime_relative_day_long_v1!(Provider);
    impl_datetime_relative_day_short_v1!(Provider);
    impl_datetime_relative_day_narrow_v1!(Provider);
    impl_datetime_relative_week_long_v1!(Provider);
    impl_datetime_relative_week_short_v1!(Provider);
    impl_datetime_relative_week_narrow_v1!(Provider);
    impl_datetime_relative_month_long_v1!(Provider);
    impl_datetime_relative_month_short_v1!(Provider);
    impl_datetime_relative_month_narrow_v1!(Provider);
    impl_datetime_relative_quarter_long_v1!(Provider);
    impl_datetime_relative_quarter_short_v1!(Provider);
    impl_datetime_relative_quarter_narrow_v1!(Provider);
    impl_datetime_relative_year_long_v1!(Provider);
    impl_datetime_relative_year_short_v1!(Provider);
    impl_datetime_relative_year_narrow_v1!(Provider);
}

type Payload = DataPayload<DatetimeRelativeSecondLongV1>;
pub struct Handle(Vec<Payload>);
#[repr(C)]
pub struct Pattern {
    prefix: Text,
    suffix: Text,
    has_number: u32,
}

fn load(locale: &Locale, width: u32, unit: u32) -> Result<Payload, i32> {
    macro_rules! row {
        ($marker:ty) => {{
            let locale = <$marker>::make_locale(locale.into());
            <Baked as DataProvider<$marker>>::load(
                &Baked,
                DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                },
            )
            .map(|response| response.payload.cast())
            .map_err(|_| BACKEND)
        }};
    }
    match (unit, width) {
        (0, 0) => row!(DatetimeRelativeSecondLongV1),
        (0, 1) => row!(DatetimeRelativeSecondShortV1),
        (0, 2) => row!(DatetimeRelativeSecondNarrowV1),
        (1, 0) => row!(DatetimeRelativeMinuteLongV1),
        (1, 1) => row!(DatetimeRelativeMinuteShortV1),
        (1, 2) => row!(DatetimeRelativeMinuteNarrowV1),
        (2, 0) => row!(DatetimeRelativeHourLongV1),
        (2, 1) => row!(DatetimeRelativeHourShortV1),
        (2, 2) => row!(DatetimeRelativeHourNarrowV1),
        (3, 0) => row!(DatetimeRelativeDayLongV1),
        (3, 1) => row!(DatetimeRelativeDayShortV1),
        (3, 2) => row!(DatetimeRelativeDayNarrowV1),
        (4, 0) => row!(DatetimeRelativeWeekLongV1),
        (4, 1) => row!(DatetimeRelativeWeekShortV1),
        (4, 2) => row!(DatetimeRelativeWeekNarrowV1),
        (5, 0) => row!(DatetimeRelativeMonthLongV1),
        (5, 1) => row!(DatetimeRelativeMonthShortV1),
        (5, 2) => row!(DatetimeRelativeMonthNarrowV1),
        (6, 0) => row!(DatetimeRelativeQuarterLongV1),
        (6, 1) => row!(DatetimeRelativeQuarterShortV1),
        (6, 2) => row!(DatetimeRelativeQuarterNarrowV1),
        (7, 0) => row!(DatetimeRelativeYearLongV1),
        (7, 1) => row!(DatetimeRelativeYearShortV1),
        (7, 2) => row!(DatetimeRelativeYearNarrowV1),
        _ => Err(RANGE),
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_relative_open(
    locale: *const u8,
    length: usize,
    width: u32,
    out: *mut *mut Handle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if locale.is_null() || length > isize::MAX as usize || width > 2 {
                return Err(RANGE);
            }
            let locale = Locale::try_from_utf8(std::slice::from_raw_parts(locale, length))
                .map_err(|_| RANGE)?;
            let mut rows = Vec::with_capacity(8);
            for unit in 0..8 {
                rows.push(load(&locale, width, unit)?);
            }
            Ok(Box::into_raw(Box::new(Handle(rows))))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_relative_close(handle: *mut Handle) {
    if !handle.is_null() {
        unsafe { drop(Box::from_raw(handle)) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_relative_pattern(
    handle: *const Handle,
    unit: u32,
    past: u32,
    category: u32,
    out: *mut Pattern,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            let row = handle.0.get(unit as usize).ok_or(RANGE)?.get();
            if past > 1 {
                return Err(RANGE);
            }
            let rules = selector(category)?;
            // Relative-time patterns have grammatical plural categories, not exact
            // numeric selectors. The synthetic operand only accesses that metadata;
            // it never replaces the exact quantity classified by the C evaluator.
            let pattern = if past != 0 { &row.past } else { &row.future }.get(2_u32.into(), &rules);
            let mut result = Pattern {
                prefix: "".into(),
                suffix: "".into(),
                has_number: 0,
            };
            for item in pattern.iter() {
                match item {
                    PatternItem::Literal(value) => {
                        let target = if result.has_number == 0 {
                            &mut result.prefix
                        } else {
                            &mut result.suffix
                        };
                        if target.length != 0 {
                            return Err(BACKEND);
                        }
                        *target = value.into();
                    }
                    PatternItem::Placeholder(_) => {
                        if result.has_number != 0 {
                            return Err(BACKEND);
                        }
                        result.has_number = 1;
                    }
                }
            }
            Ok(result)
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_relative_literal(
    handle: *const Handle,
    unit: u32,
    offset: i32,
    out: *mut Text,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            let row = handle.0.get(unit as usize).ok_or(RANGE)?.get();
            let offset = i8::try_from(offset).map_err(|_| UNSUPPORTED)?;
            row.get_relative(offset).map(Text::from).ok_or(UNSUPPORTED)
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_relative_check_version(data: *const u8, length: usize) -> i32 {
    if data.is_null() || length > isize::MAX as usize {
        return RANGE;
    }
    if unsafe { std::slice::from_raw_parts(data, length) }
        == icu_experimental_data::cldr_tag!().as_bytes()
    {
        0
    } else {
        BACKEND
    }
}
