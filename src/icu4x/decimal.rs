/* Private ICU4X decimal metadata adapter.
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
use icu_decimal::CompactDecimalFormatter;
use icu_locale_core::Locale;

pub struct CompactHandle(CompactDecimalFormatter);

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_compact_open(
    locale: *const u8,
    length: usize,
    long: u32,
    out: *mut *mut CompactHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if locale.is_null() || length > isize::MAX as usize || long > 1 {
                return Err(RANGE);
            }
            let locale = Locale::try_from_utf8(std::slice::from_raw_parts(locale, length))
                .map_err(|_| RANGE)?;
            let formatter = if long != 0 {
                CompactDecimalFormatter::try_new_long(locale.into(), Default::default())
            } else {
                CompactDecimalFormatter::try_new_short(locale.into(), Default::default())
            }
            .map_err(|_| BACKEND)?;
            Ok(Box::into_raw(Box::new(CompactHandle(formatter))))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_compact_close(handle: *mut CompactHandle) {
    if !handle.is_null() {
        unsafe {
            drop(Box::from_raw(handle));
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_compact_exponent(
    handle: *const CompactHandle,
    magnitude: i32,
    out: *mut i32,
) -> i32 {
    unsafe {
        write_result(out, || {
            if handle.is_null() {
                return Err(RANGE);
            }
            // ICU4X compact thresholds have u8 magnitudes. Outside i16 all
            // thresholds have either been passed or not reached; clamp this
            // metadata lookup only, never the exact mathematical input itself.
            let magnitude = magnitude.clamp(i16::MIN as i32, i16::MAX as i32) as i16;
            Ok(i32::from(
                (&*handle).0.compact_exponent_for_magnitude(magnitude),
            ))
        })
    }
}

use icu_decimal::provider::{DecimalDigitsV1, DecimalSymbolsV1};
use icu_decimal::{DecimalFormatter, DecimalFormatterPreferences};
use icu_locale_core::subtags::Subtag;
use icu_provider::{DataError, DataPayload, DataProvider, DataRequest, DataResponse};
use std::cell::RefCell;

#[derive(Default)]
struct Capture {
    symbols: RefCell<Option<DataPayload<DecimalSymbolsV1>>>,
    digits: RefCell<Option<(DataPayload<DecimalDigitsV1>, String)>>,
}

// Let the public constructor resolve locale/numbering-system fallback. Capture
// only successful responses, retaining the same payloads and actual digit ID;
// failed speculative requests must not replace the eventual resolved result.
impl DataProvider<DecimalSymbolsV1> for Capture {
    fn load(&self, request: DataRequest) -> Result<DataResponse<DecimalSymbolsV1>, DataError> {
        let response = <inventory::Provider as DataProvider<DecimalSymbolsV1>>::load(
            &inventory::Provider,
            request,
        )?;
        self.symbols.replace(Some(response.payload.clone()));
        Ok(response)
    }
}
impl DataProvider<DecimalDigitsV1> for Capture {
    fn load(&self, request: DataRequest) -> Result<DataResponse<DecimalDigitsV1>, DataError> {
        let response = <inventory::Provider as DataProvider<DecimalDigitsV1>>::load(
            &inventory::Provider,
            request,
        )?;
        self.digits.replace(Some((
            response.payload.clone(),
            request.id.marker_attributes.as_str().to_owned(),
        )));
        Ok(response)
    }
}

pub struct DecimalHandle {
    symbols: DataPayload<DecimalSymbolsV1>,
    digits: DataPayload<DecimalDigitsV1>,
    system: String,
}
#[repr(C)]
pub struct DecimalText {
    data: *const u8,
    length: usize,
}
#[repr(C)]
pub struct DecimalSymbols {
    digits: [u32; 10],
    primary: u32,
    secondary: u32,
    minimum: u32,
    fields: [DecimalText; 8],
}

unsafe fn decimal_input<'a>(data: *const u8, length: usize) -> Result<&'a [u8], i32> {
    if length > isize::MAX as usize || (length != 0 && data.is_null()) {
        return Err(RANGE);
    }
    if length == 0 {
        Ok(&[])
    } else {
        Ok(unsafe { std::slice::from_raw_parts(data, length) })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_decimal_open(
    locale: *const u8,
    locale_length: usize,
    system: *const u8,
    system_length: usize,
    out: *mut *mut DecimalHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            let locale =
                Locale::try_from_utf8(decimal_input(locale, locale_length)?).map_err(|_| RANGE)?;
            let system = decimal_input(system, system_length)?;
            let mut prefs: DecimalFormatterPreferences = locale.into();
            if !system.is_empty() {
                let subtag = Subtag::try_from_utf8(system).map_err(|_| RANGE)?;
                prefs.numbering_system = Some(
                    icu_decimal::preferences::NumberingSystem::try_from(
                        icu_locale_core::extensions::unicode::Value::from_subtag(Some(subtag)),
                    )
                    .map_err(|_| RANGE)?,
                );
            }
            let capture = Capture::default();
            DecimalFormatter::try_new_unstable(&capture, prefs, Default::default())
                .map_err(|_| BACKEND)?;
            let symbols = capture.symbols.into_inner().ok_or(BACKEND)?;
            let (digits, resolved) = capture.digits.into_inner().ok_or(BACKEND)?;
            if !system.is_empty() && system != resolved.as_bytes() {
                return Err(crate::UNSUPPORTED);
            }
            Ok(Box::into_raw(Box::new(DecimalHandle {
                symbols,
                digits,
                system: resolved,
            })))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_decimal_close(handle: *mut DecimalHandle) {
    if !handle.is_null() {
        unsafe {
            drop(Box::from_raw(handle));
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_decimal_symbols(
    handle: *const DecimalHandle,
    out: *mut DecimalSymbols,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            let symbols = handle.symbols.get();
            let grouping = symbols.grouping_sizes;
            let fields = [
                symbols.decimal_separator(),
                symbols.grouping_separator(),
                symbols.minus_sign_prefix(),
                symbols.minus_sign_suffix(),
                symbols.plus_sign_prefix(),
                symbols.plus_sign_suffix(),
                &handle.system,
                symbols.numsys(),
            ];
            Ok(DecimalSymbols {
                digits: handle.digits.get().map(u32::from),
                primary: grouping.primary.into(),
                secondary: if grouping.primary == 0 {
                    0
                } else if grouping.secondary == 0 {
                    grouping.primary.into()
                } else {
                    grouping.secondary.into()
                },
                minimum: grouping.min_grouping.into(),
                fields: fields.map(|text| DecimalText {
                    data: text.as_ptr(),
                    length: text.len(),
                }),
            })
        })
    }
}

// The same public generated payloads establish executable numeric identities;
// no list of numbering-system metadata is mistaken for actual digit coverage.
mod inventory {
    use icu_decimal_data::*;
    pub mod icu {
        pub use icu_decimal as decimal;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_decimal_symbols_v1!(Provider);
    impl_decimal_digits_v1!(Provider, ITER);
}

fn available_systems() -> Result<&'static std::collections::BTreeSet<String>, i32> {
    use icu_provider::IterableDataProvider;
    use std::{collections::BTreeSet, sync::OnceLock};
    static SYSTEMS: OnceLock<Result<BTreeSet<String>, i32>> = OnceLock::new();
    SYSTEMS
        .get_or_init(|| {
            let mut systems = BTreeSet::new();
            for id in <inventory::Provider as IterableDataProvider<DecimalDigitsV1>>::iter_ids(
                &inventory::Provider,
            )
            .map_err(|_| BACKEND)?
            {
                let system = id.marker_attributes.as_str();
                if system.is_empty() {
                    return Err(BACKEND);
                }
                systems.insert(system.to_owned());
            }
            Ok(systems)
        })
        .as_ref()
        .map_err(|status| *status)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_decimal_systems(
    opaque: *mut std::ffi::c_void,
    emit: unsafe extern "C" fn(*mut std::ffi::c_void, *const u8, usize) -> i32,
) -> i32 {
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let values = available_systems()?;
        for value in values {
            let status = unsafe { emit(opaque, value.as_ptr(), value.len()) };
            if status != 0 {
                return Err(status);
            }
        }
        Ok(())
    })) {
        Ok(Ok(())) => 0,
        Ok(Err(status)) => status,
        Err(_) => BACKEND,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_decimal_check_version(text: *const u8, length: usize) -> i32 {
    if text.is_null() || length > isize::MAX as usize {
        return RANGE;
    }
    if unsafe { std::slice::from_raw_parts(text, length) }
        == icu_decimal_data::cldr_tag!().as_bytes()
    {
        0
    } else {
        BACKEND
    }
}
