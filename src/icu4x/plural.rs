/* Private ICU4X plural source-data adapter.
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
use crate::{BACKEND, RANGE, write_result};
use icu_locale_core::Locale;
use icu_plurals::provider::{Baked, PluralRulesData, PluralsCardinalV1, PluralsOrdinalV1};
use icu_plurals::{
    PluralCategory, PluralRuleType, PluralRules, PluralRulesOptions, PluralRulesPreferences,
    PluralRulesWithRanges,
};
use icu_provider::marker::DataMarkerExt;
use icu_provider::{DataIdentifierBorrowed, DataProvider, DataRequest};

pub struct PluralHandle {
    rules: [String; 6],
    mask: u32,
    ranges: [u8; 36],
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct RuleBytes {
    data: *const u8,
    length: usize,
}
#[repr(C)]
pub struct PluralData {
    rules: [RuleBytes; 6],
    mask: u32,
    ranges: [u8; 36],
}

fn category(index: usize) -> PluralCategory {
    match index {
        0 => PluralCategory::Zero,
        1 => PluralCategory::One,
        2 => PluralCategory::Two,
        3 => PluralCategory::Few,
        4 => PluralCategory::Many,
        _ => PluralCategory::Other,
    }
}
fn category_index(value: PluralCategory) -> u8 {
    match value {
        PluralCategory::Zero => 0,
        PluralCategory::One => 1,
        PluralCategory::Two => 2,
        PluralCategory::Few => 3,
        PluralCategory::Many => 4,
        PluralCategory::Other => 5,
    }
}
fn snapshot(
    data: &PluralRulesData<'_>,
    range: &PluralRulesWithRanges<PluralRules>,
) -> PluralHandle {
    let source = [&data.zero, &data.one, &data.two, &data.few, &data.many];
    let mut result = PluralHandle {
        rules: std::array::from_fn(|_| String::new()),
        mask: 1 << 5,
        ranges: [0; 36],
    };
    for (index, rule) in source.into_iter().enumerate() {
        if let Some(rule) = rule {
            // Public ICU4X rule serialization retains the complete CLDR
            // relation. Do not convert exact operands through u64 or f64.
            result.rules[index] = rule.to_string();
            result.mask |= 1 << index;
        }
    }
    for start in 0..6 {
        for end in 0..6 {
            result.ranges[start * 6 + end] =
                category_index(range.resolve_range(category(start), category(end)));
        }
    }
    result
}
unsafe fn open(
    locale: *const u8,
    length: usize,
    range_locale: *const u8,
    range_length: usize,
    kind: u32,
) -> Result<PluralHandle, i32> {
    if locale.is_null() || length > isize::MAX as usize || kind > 1 {
        return Err(RANGE);
    }
    let locale = Locale::try_from_utf8(unsafe { std::slice::from_raw_parts(locale, length) })
        .map_err(|_| RANGE)?;
    let prefs: PluralRulesPreferences = locale.into();
    // Match the public constructor's subdivision/region preference projection,
    // rather than resolving rules and ranges from different locale identities.
    let data_locale = if kind == 0 {
        PluralsCardinalV1::make_locale(prefs.locale_preferences)
    } else {
        PluralsOrdinalV1::make_locale(prefs.locale_preferences)
    };
    let request = DataRequest {
        id: DataIdentifierBorrowed::for_locale(&data_locale),
        ..Default::default()
    };
    let options = PluralRulesOptions::default().with_type(if kind == 0 {
        PluralRuleType::Cardinal
    } else {
        PluralRuleType::Ordinal
    });
    if range_locale.is_null() || range_length > isize::MAX as usize {
        return Err(RANGE);
    }
    let range_locale =
        Locale::try_from_utf8(unsafe { std::slice::from_raw_parts(range_locale, range_length) })
            .map_err(|_| RANGE)?;
    let range =
        PluralRulesWithRanges::try_new(range_locale.into(), options).map_err(|_| BACKEND)?;
    if kind == 0 {
        let data = <Baked as DataProvider<PluralsCardinalV1>>::load(&Baked, request)
            .map_err(|_| BACKEND)?
            .payload;
        Ok(snapshot(data.get(), &range))
    } else {
        let data = <Baked as DataProvider<PluralsOrdinalV1>>::load(&Baked, request)
            .map_err(|_| BACKEND)?
            .payload;
        Ok(snapshot(data.get(), &range))
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_plural_open(
    locale: *const u8,
    length: usize,
    kind: u32,
    out: *mut *mut PluralHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            Ok(Box::into_raw(Box::new(open(
                locale, length, locale, length, kind,
            )?)))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_plural_close(handle: *mut PluralHandle) {
    if !handle.is_null() {
        unsafe {
            drop(Box::from_raw(handle));
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_plural_data(
    handle: *const PluralHandle,
    out: *mut PluralData,
) -> i32 {
    unsafe {
        write_result(out, || {
            if handle.is_null() {
                return Err(RANGE);
            }
            let handle = &*handle;
            Ok(PluralData {
                rules: std::array::from_fn(|index| RuleBytes {
                    data: handle.rules[index].as_ptr(),
                    length: handle.rules[index].len(),
                }),
                mask: handle.mask,
                ranges: handle.ranges,
            })
        })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_plural_check_version(data: *const u8, length: usize) -> i32 {
    if data.is_null() || length > isize::MAX as usize {
        return RANGE;
    }
    if unsafe { std::slice::from_raw_parts(data, length) }
        == icu_plurals_data::cldr_tag!().as_bytes()
    {
        0
    } else {
        BACKEND
    }
}

// Rule and range component inheritance can select different CLDR sources.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_plural_open_sources(
    locale: *const u8,
    length: usize,
    range_locale: *const u8,
    range_length: usize,
    kind: u32,
    out: *mut *mut PluralHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            Ok(Box::into_raw(Box::new(open(
                locale,
                length,
                range_locale,
                range_length,
                kind,
            )?)))
        })
    }
}
