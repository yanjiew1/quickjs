/* Private ICU4X currency numeric-data adapter.
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
use icu_experimental::dimension::{
    currency::CurrencyType,
    provider::currency::{
        fractions::CurrencyFractionsV1,
        symbols::{CurrencySymbolWidth, CurrencySymbolsV1},
    },
};
use icu_locale_core::Locale;
use icu_provider::{
    DataErrorKind, DataIdentifierBorrowed, DataPayload, DataProvider, DataRequest,
    marker::DataMarkerExt,
};

// Enable only the two public currency payload families used here. The broad
// experimental compiled_data feature would include unrelated formatters/data.
mod source {
    use icu_experimental_data::*;
    pub mod icu {
        pub use icu_experimental as experimental;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_currency_fractions_v1!(Provider);
    impl_currency_symbols_v1!(Provider);
}

unsafe fn input<'a>(data: *const u8, length: usize) -> Result<&'a [u8], i32> {
    if data.is_null() || length > isize::MAX as usize {
        return Err(RANGE);
    }
    Ok(unsafe { std::slice::from_raw_parts(data, length) })
}
fn currency(code: &[u8]) -> Result<CurrencyType, i32> {
    if code.len() != 3 || !code.iter().all(u8::is_ascii_uppercase) {
        return Err(RANGE);
    }
    CurrencyType::try_from_utf8(code).map_err(|_| RANGE)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_currency_digits(
    code: *const u8,
    length: usize,
    out: *mut u32,
) -> i32 {
    unsafe {
        write_result(out, || {
            let code = currency(input(code, length)?)?;
            let data: DataPayload<CurrencyFractionsV1> = source::Provider
                .load(Default::default())
                .map_err(|_| BACKEND)?
                .payload;
            // CurrencyDigits is regular ISO precision, never cash digits or cash
            // rounding. Unknown well-formed codes use the selected data's default.
            Ok(u32::from(data.get().resolve(code).digits))
        })
    }
}

pub struct SymbolHandle {
    payload: Option<DataPayload<CurrencySymbolsV1>>,
}
#[repr(C)]
pub struct CurrencySymbol {
    text: *const u8,
    length: usize,
    starts_with_letter: u32,
    ends_with_letter: u32,
    found: u32,
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_currency_symbol_open(
    locale: *const u8,
    locale_length: usize,
    code: *const u8,
    code_length: usize,
    narrow: u32,
    out: *mut *mut SymbolHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if narrow > 1 {
                return Err(RANGE);
            }
            let locale = Locale::try_from_utf8(input(locale, locale_length)?).map_err(|_| RANGE)?;
            let currency = currency(input(code, code_length)?)?;
            let locale = CurrencySymbolsV1::make_locale((&locale).into());
            let width = if narrow != 0 {
                CurrencySymbolWidth::Narrow
            } else {
                CurrencySymbolWidth::Short
            };
            let response = <source::Provider as DataProvider<CurrencySymbolsV1>>::load(
                &source::Provider,
                DataRequest {
                    id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                        CurrencySymbolsV1::make_attributes(
                            currency,
                            width,
                            &mut Default::default(),
                        ),
                        &locale,
                    ),
                    ..Default::default()
                },
            );
            let payload = match response {
                Ok(response) => Some(response.payload),
                Err(error) if error.kind == DataErrorKind::IdentifierNotFound => None,
                Err(_) => return Err(BACKEND),
            };
            Ok(Box::into_raw(Box::new(SymbolHandle { payload })))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_currency_symbol_close(handle: *mut SymbolHandle) {
    if !handle.is_null() {
        unsafe {
            drop(Box::from_raw(handle));
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_currency_symbol(
    handle: *const SymbolHandle,
    out: *mut CurrencySymbol,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            if let Some(payload) = &handle.payload {
                let symbol = payload.get();
                Ok(CurrencySymbol {
                    text: symbol.as_str().as_ptr(),
                    length: symbol.as_str().len(),
                    starts_with_letter: symbol.starts_with_letter().into(),
                    ends_with_letter: symbol.ends_with_letter().into(),
                    found: 1,
                })
            } else {
                // Do not return a pointer to a temporary uppercased ISO code.
                // The C owner retains the original canonical code for fallback.
                Ok(CurrencySymbol {
                    text: std::ptr::null(),
                    length: 0,
                    starts_with_letter: 0,
                    ends_with_letter: 0,
                    found: 0,
                })
            }
        })
    }
}
