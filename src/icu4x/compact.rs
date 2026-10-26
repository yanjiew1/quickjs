/* Private ICU4X compact-pattern metadata adapter.
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
use crate::plural_pattern::{SinglePattern, selector, single_pattern};
use crate::{BACKEND, RANGE, write_result};
use icu_decimal::{
    DecimalFormatterPreferences,
    provider::{DecimalCompactLongV1, DecimalCompactShortV1},
};
use icu_locale_core::{Locale, subtags::Subtag};
use icu_provider::{
    DataErrorKind, DataIdentifierBorrowed, DataPayload, DataProvider, DataRequest,
    marker::DataMarkerExt,
};

mod source {
    use icu_decimal_data::*;
    pub mod icu {
        pub use icu_decimal as decimal;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_decimal_compact_short_v1!(Provider);
    impl_decimal_compact_long_v1!(Provider);
}

pub struct CompactPatternsHandle(DataPayload<DecimalCompactShortV1>);
#[repr(C)]
pub struct Scale {
    magnitude: u32,
    exponent: u32,
}
pub type CompactPattern = SinglePattern;

unsafe fn input<'a>(data: *const u8, length: usize) -> Result<&'a [u8], i32> {
    if length > isize::MAX as usize || (length != 0 && data.is_null()) {
        return Err(RANGE);
    }
    Ok(if length == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(data, length) }
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_compact_patterns_open(
    locale: *const u8,
    length: usize,
    system: *const u8,
    system_length: usize,
    long: u32,
    out: *mut *mut CompactPatternsHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if long > 1 {
                return Err(RANGE);
            }
            let locale = Locale::try_from_utf8(input(locale, length)?).map_err(|_| RANGE)?;
            let mut prefs: DecimalFormatterPreferences = locale.into();
            let system = input(system, system_length)?;
            if !system.is_empty() {
                let subtag = Subtag::try_from_utf8(system).map_err(|_| RANGE)?;
                prefs.numbering_system = Some(
                    icu_decimal::preferences::NumberingSystem::try_from(
                        icu_locale_core::extensions::unicode::Value::from_subtag(Some(subtag)),
                    )
                    .map_err(|_| RANGE)?,
                );
            }
            let locale = DecimalCompactShortV1::make_locale(prefs.locale_preferences);
            // Match the public compact constructor's exact-nu then unqualified
            // pattern lookup. The caller localizes digits separately; an absent
            // nu-specific pattern is not an unsupported digit repertoire.
            let ids = prefs
                .nu_id(&locale)
                .into_iter()
                .chain([DataIdentifierBorrowed::for_locale(&locale)]);
            for id in ids {
                let request = DataRequest {
                    id,
                    ..Default::default()
                };
                let data = if long != 0 {
                    <source::Provider as DataProvider<DecimalCompactLongV1>>::load(
                        &source::Provider,
                        request,
                    )
                    .map(|data| data.payload.cast())
                } else {
                    <source::Provider as DataProvider<DecimalCompactShortV1>>::load(
                        &source::Provider,
                        request,
                    )
                    .map(|data| data.payload)
                };
                match data {
                    Ok(payload) => {
                        return Ok(Box::into_raw(Box::new(CompactPatternsHandle(payload))));
                    }
                    Err(error) if error.kind == DataErrorKind::IdentifierNotFound => {}
                    Err(_) => return Err(BACKEND),
                }
            }
            Err(BACKEND)
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_compact_patterns_close(handle: *mut CompactPatternsHandle) {
    if !handle.is_null() {
        unsafe {
            drop(Box::from_raw(handle));
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_compact_patterns_count(
    handle: *const CompactPatternsHandle,
    out: *mut u32,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            handle.0.get().0.len().try_into().map_err(|_| BACKEND)
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_compact_patterns_scale(
    handle: *const CompactPatternsHandle,
    index: u32,
    out: *mut Scale,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            let row = handle.0.get().0.get(index as usize).ok_or(RANGE)?;
            // The public formatter derives the scale from default/other metadata,
            // independently of the later category/explicit text selection.
            let exponent = row
                .sized
                .checked_sub(row.variable.get_default().0.get())
                .ok_or(BACKEND)?;
            Ok(Scale {
                magnitude: row.sized.into(),
                exponent: exponent.into(),
            })
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_compact_patterns_get(
    handle: *const CompactPatternsHandle,
    index: u32,
    category: u32,
    exact: i32,
    out: *mut CompactPattern,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            if !(-1..=1).contains(&exact) {
                return Err(RANGE);
            }
            let rules = selector(category)?;
            let row = handle.0.get().0.get(index as usize).ok_or(RANGE)?;
            let (_, pattern) = row
                .variable
                .get((if exact < 0 { 2 } else { exact as u32 }).into(), &rules);
            single_pattern(pattern)
        })
    }
}
