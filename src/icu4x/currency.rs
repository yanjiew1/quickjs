/* Private ICU4X currency source-data adapter.
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
use crate::{BACKEND, RANGE, UNSUPPORTED};
use icu_experimental::dimension::provider::currency::displayname::CurrencyDisplaynameV1;
use icu_locale_core::preferences::LocalePreferences;
use icu_provider::{
    DataErrorKind, DataIdentifierBorrowed, DataMarkerAttributes, DataPayload, DataProvider,
    DataRequest, marker::DataMarkerExt,
};
use std::{
    collections::BTreeSet,
    ffi::c_void,
    panic::{AssertUnwindSafe, catch_unwind},
    sync::OnceLock,
};

// Only plain-name IDs establish this inventory. Currency metadata, symbols
// and plural unit patterns cannot establish DisplayNames name presence.
mod inventory {
    use icu_experimental_data::*;
    pub mod icu {
        pub use icu_experimental as experimental;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_currency_displayname_v1!(Provider, ITER);
}

pub(crate) fn plain_name(
    prefs: LocalePreferences,
    code: &[u8],
) -> Result<DataPayload<CurrencyDisplaynameV1>, i32> {
    if code.len() != 3 || !code.iter().all(u8::is_ascii_uppercase) {
        return Err(RANGE);
    }
    let code = std::str::from_utf8(code).map_err(|_| RANGE)?;
    let attributes = DataMarkerAttributes::try_from_str(code).map_err(|_| RANGE)?;
    let locale = CurrencyDisplaynameV1::make_locale(prefs);
    <inventory::Provider as DataProvider<CurrencyDisplaynameV1>>::load(
        &inventory::Provider,
        DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes_and_locale(attributes, &locale),
            ..Default::default()
        },
    )
    .map(|data| data.payload)
    .map_err(|error| {
        if error.kind == DataErrorKind::IdentifierNotFound {
            UNSUPPORTED
        } else {
            BACKEND
        }
    })
}

fn available() -> Result<&'static BTreeSet<String>, i32> {
    use icu_provider::IterableDataProvider;
    static CODES: OnceLock<Result<BTreeSet<String>, i32>> = OnceLock::new();
    CODES
        .get_or_init(|| {
            let ids =
                <inventory::Provider as IterableDataProvider<CurrencyDisplaynameV1>>::iter_ids(
                    &inventory::Provider,
                )
                .map_err(|_| BACKEND)?;
            let mut result = BTreeSet::new();
            for id in ids {
                let code = id.marker_attributes.as_str();
                if code.len() != 3 || !code.bytes().all(|c| c.is_ascii_uppercase()) {
                    return Err(BACKEND);
                }
                result.insert(code.to_owned());
            }
            Ok(result)
        })
        .as_ref()
        .map_err(|status| *status)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_currency_names(
    opaque: *mut c_void,
    emit: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> i32,
) -> i32 {
    match catch_unwind(AssertUnwindSafe(|| {
        for code in available()? {
            let status = unsafe { emit(opaque, code.as_ptr(), code.len()) };
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
pub unsafe extern "C" fn qjs_icu4x_currency_present(
    locale: *const u8,
    locale_length: usize,
    code: *const u8,
    code_length: usize,
) -> i32 {
    match catch_unwind(|| {
        if locale.is_null()
            || code.is_null()
            || locale_length > isize::MAX as usize
            || code_length > isize::MAX as usize
        {
            return Err(RANGE);
        }
        let locale = icu_locale_core::Locale::try_from_utf8(unsafe {
            std::slice::from_raw_parts(locale, locale_length)
        })
        .map_err(|_| RANGE)?;
        plain_name((&locale).into(), unsafe {
            std::slice::from_raw_parts(code, code_length)
        })?;
        Ok(())
    }) {
        Ok(Ok(())) => 0,
        Ok(Err(status)) => status,
        Err(_) => BACKEND,
    }
}
