/* Private ICU4X plural currency-name and pattern metadata.
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
use icu_experimental::dimension::provider::currency::{
    displayname::CurrencyDisplaynameV1, extended::CurrencyExtendedDataV1,
    patterns::CurrencyPatternsDataV1,
};
use icu_locale_core::Locale;
use icu_pattern::{DoublePlaceholderKey, DoublePlaceholderPattern, PatternItem};
use icu_provider::{
    DataErrorKind, DataIdentifierBorrowed, DataMarkerAttributes, DataPayload, DataProvider,
    DataRequest, marker::DataMarkerExt,
};

mod source {
    use icu_experimental_data::*;
    pub mod icu {
        pub use icu_experimental as experimental;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_currency_extended_data_v1!(Provider);
    impl_currency_patterns_data_v1!(Provider);
}

pub struct CurrencyNameHandle {
    name: Option<DataPayload<CurrencyExtendedDataV1>>,
    plain: Option<DataPayload<CurrencyDisplaynameV1>>,
    pattern: DataPayload<CurrencyPatternsDataV1>,
    code: [u8; 3],
}
#[repr(C)]
pub struct CurrencyName {
    name: Text,
    literals: [Text; 3],
    first: u32,
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_currency_name_open(
    locale: *const u8,
    locale_length: usize,
    code: *const u8,
    code_length: usize,
    out: *mut *mut CurrencyNameHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if locale.is_null()
                || code.is_null()
                || locale_length > isize::MAX as usize
                || code_length != 3
            {
                return Err(RANGE);
            }
            let locale = Locale::try_from_utf8(std::slice::from_raw_parts(locale, locale_length))
                .map_err(|_| RANGE)?;
            let code = std::slice::from_raw_parts(code, code_length);
            if !code.iter().all(u8::is_ascii_uppercase) {
                return Err(RANGE);
            }
            let code_text = std::str::from_utf8(code).map_err(|_| RANGE)?;
            let attributes = DataMarkerAttributes::try_from_str(code_text).map_err(|_| RANGE)?;
            let name_locale = CurrencyExtendedDataV1::make_locale((&locale).into());
            let name = match <source::Provider as DataProvider<CurrencyExtendedDataV1>>::load(
                &source::Provider,
                DataRequest {
                    id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                        attributes,
                        &name_locale,
                    ),
                    ..Default::default()
                },
            ) {
                Ok(data) => Some(data.payload),
                Err(error) if error.kind == DataErrorKind::IdentifierNotFound => None,
                Err(_) => return Err(BACKEND),
            };
            let plain = match crate::currency::plain_name((&locale).into(), code) {
                Ok(data) => Some(data),
                Err(UNSUPPORTED) => None,
                Err(error) => return Err(error),
            };
            let pattern_locale = CurrencyPatternsDataV1::make_locale((&locale).into());
            let pattern = <source::Provider as DataProvider<CurrencyPatternsDataV1>>::load(
                &source::Provider,
                DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&pattern_locale),
                    ..Default::default()
                },
            )
            .map_err(|_| BACKEND)?
            .payload;
            Ok(Box::into_raw(Box::new(CurrencyNameHandle {
                name,
                plain,
                pattern,
                code: code.try_into().map_err(|_| RANGE)?,
            })))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_currency_name_close(handle: *mut CurrencyNameHandle) {
    if !handle.is_null() {
        unsafe {
            drop(Box::from_raw(handle));
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_currency_name(
    handle: *const CurrencyNameHandle,
    category: u32,
    exact: i32,
    out: *mut CurrencyName,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            if category >= 6 || !(-1..=1).contains(&exact) {
                return Err(RANGE);
            }
            let rules = selector(category)?;
            let operands = (if exact < 0 { 2 } else { exact as u32 }).into();
            let name = if let Some(name) = &handle.name {
                name.get().get(operands, &rules)
            } else if let Some(name) = &handle.plain {
                name.get().as_ref()
            } else {
                std::str::from_utf8(&handle.code).map_err(|_| BACKEND)?
            };
            let pattern = handle.pattern.get().get(operands, &rules);
            pattern_parts(name, pattern)
        })
    }
}

// Project the public item iterator directly: literal braces must not acquire
// placeholder semantics when composing typed number/currency parts.
fn pattern_parts(name: &str, pattern: &DoublePlaceholderPattern) -> Result<CurrencyName, i32> {
    let mut result = CurrencyName {
        name: name.into(),
        literals: [Text::from(""); 3],
        first: 0,
    };
    let mut count = 0;
    for item in pattern.iter() {
        match item {
            PatternItem::Literal(value) => {
                if count > 2 || result.literals[count].length != 0 {
                    return Err(BACKEND);
                }
                result.literals[count] = value.into();
            }
            PatternItem::Placeholder(key) => {
                let key = match key {
                    DoublePlaceholderKey::Place0 => 0,
                    DoublePlaceholderKey::Place1 => 1,
                };
                if count == 0 {
                    result.first = key;
                } else if count != 1 || key == result.first {
                    return Err(BACKEND);
                }
                count += 1;
            }
        }
    }
    if count != 2 {
        return Err(BACKEND);
    }
    Ok(result)
}

#[cfg(test)]
mod tests {
    use super::*;
    use icu_plurals::{PluralElements, provider::PluralElementsPackedCow};

    #[test]
    fn explicit_metadata_does_not_replace_category_selection() {
        let packed: PluralElementsPackedCow<'static, str> = PluralElements::new("other")
            .with_one_value(Some("one"))
            .with_explicit_zero_value(Some("exact-zero"))
            .with_explicit_one_value(Some("exact-one"))
            .into();
        let one = selector(1).unwrap();
        let other = selector(5).unwrap();
        assert_eq!(packed.get(0u32.into(), &other), "exact-zero");
        assert_eq!(packed.get(1u32.into(), &other), "exact-one");
        assert_eq!(packed.get(2u32.into(), &one), "one");
        assert_eq!(packed.get(2u32.into(), &other), "other");
        let fallback: PluralElementsPackedCow<'static, str> = PluralElements::new("other")
            .with_one_value(Some("one"))
            .into();
        assert_eq!(fallback.get(0u32.into(), &one), "one");
        assert_eq!(fallback.get(1u32.into(), &other), "other");
    }

    #[test]
    fn reversed_pattern_preserves_literal_braces() {
        let pattern = DoublePlaceholderPattern::try_from_items(
            [
                PatternItem::Literal("{0}"),
                PatternItem::Placeholder(DoublePlaceholderKey::Place1),
                PatternItem::Literal(" then "),
                PatternItem::Placeholder(DoublePlaceholderKey::Place0),
                PatternItem::Literal("}"),
            ]
            .into_iter()
            .map(Into::into),
        )
        .unwrap();
        let result = pattern_parts("name", &pattern).unwrap();
        assert_eq!(result.first, 1);
        for (field, expected) in result.literals.iter().zip(["{0}", " then ", "}"]) {
            assert_eq!(
                unsafe { std::slice::from_raw_parts(field.data, field.length) },
                expected.as_bytes()
            );
        }
    }
}
