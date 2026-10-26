/* Private ICU4X digital-duration metadata.
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
use crate::{RANGE, write_result};
use icu_experimental::duration::provider::UnitsDurationDigitalV1;
use icu_locale_core::Locale;
use icu_provider::{
    DataIdentifierBorrowed, DataPayload, DataProvider, DataRequest, marker::DataMarkerExt,
};

mod source {
    extern crate alloc;
    use icu_experimental_data::*;
    pub mod icu {
        pub use icu_experimental as experimental;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_units_duration_digital_v1!(Provider);
}

pub struct Handle(DataPayload<UnitsDurationDigitalV1>);
#[repr(C)]
pub struct Digital {
    separator: *const u8,
    length: usize,
    hour_width: u32,
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_duration_open(
    locale: *const u8,
    length: usize,
    out: *mut *mut Handle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if locale.is_null() || length == 0 || length > isize::MAX as usize {
                return Err(RANGE);
            }
            let bytes = std::slice::from_raw_parts(locale, length);
            let locale = Locale::try_from_utf8(bytes).map_err(|_| RANGE)?;
            let locale = UnitsDurationDigitalV1::make_locale((&locale).into());
            let payload = source::Provider
                .load(DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                })
                .map_err(|_| crate::BACKEND)?
                .payload;
            Ok(Box::into_raw(Box::new(Handle(payload))))
        })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_duration_close(handle: *mut Handle) {
    if !handle.is_null() {
        unsafe { drop(Box::from_raw(handle)) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_duration_digital(
    handle: *const Handle,
    out: *mut Digital,
) -> i32 {
    unsafe {
        write_result(out, || {
            let data = handle.as_ref().ok_or(RANGE)?.0.get();
            Ok(Digital {
                separator: data.separator.as_ptr(),
                length: data.separator.len(),
                hour_width: data.hms_padding.h.into(),
            })
        })
    }
}
