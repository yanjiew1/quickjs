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
