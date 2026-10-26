/* Private ICU4X collation bridge.
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
use crate::{BACKEND, RANGE, UNSUPPORTED, write_result};
use icu_collator::options::{AlternateHandling, CaseLevel, CollatorOptions, MaxVariable, Strength};
use icu_collator::preferences::{CollationCaseFirst, CollationNumericOrdering, CollationType};
use icu_collator::provider::*;
use icu_collator::{Collator, CollatorPreferences};
use icu_locale_core::{Locale, extensions::unicode::Value};
use icu_provider::marker::DataMarkerExt;
use icu_provider::{DataError, DataIdentifierBorrowed, DataProvider, DataRequest, DataResponse};
use std::{collections::BTreeSet, ffi::c_void, sync::OnceLock};

mod inventory {
    use icu_collator_data::*;
    pub mod icu {
        pub use icu_collator as collator;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_collation_metadata_v1!(Provider, ITER);
}

fn types() -> Result<&'static BTreeSet<String>, i32> {
    use icu_provider::IterableDataProvider;
    static TYPES: OnceLock<Result<BTreeSet<String>, i32>> = OnceLock::new();
    TYPES
        .get_or_init(|| {
            let mut result = BTreeSet::new();
            for id in <inventory::Provider as IterableDataProvider<CollationMetadataV1>>::iter_ids(
                &inventory::Provider,
            )
            .map_err(|_| BACKEND)?
            {
                let name = id.marker_attributes.as_str();
                if !name.is_empty() && name != "standard" && name != "search" {
                    result.insert(name.to_owned());
                }
            }
            Ok(result)
        })
        .as_ref()
        .map_err(|status| *status)
}

// The public Collator constructor retries failed named requests without their
// co attribute. Reject those retries: constructor success must mean the chosen
// tailoring actually exists. Normal root/normalizer and default diacritics
// requests remain valid; no private metadata bit layout is inspected here.
struct Strict<'a> {
    expected: &'a str,
}
macro_rules! delegate {
    ($marker:ty, $provider:expr, $strict:expr) => {
        impl DataProvider<$marker> for Strict<'_> {
            fn load(&self, request: DataRequest) -> Result<DataResponse<$marker>, DataError> {
                if $strict && request.id.marker_attributes.as_str() != self.expected {
                    return Err(DataError::custom("named collation fallback disabled"));
                }
                DataProvider::<$marker>::load(&$provider, request)
            }
        }
    };
}
delegate!(CollationMetadataV1, Baked, true);
delegate!(CollationTailoringV1, Baked, true);
delegate!(CollationReorderingV1, Baked, true);
delegate!(CollationDiacriticsV1, Baked, false);
delegate!(CollationRootV1, Baked, false);
delegate!(CollationJamoV1, Baked, false);
delegate!(CollationSpecialPrimariesV1, Baked, false);
delegate!(
    icu_normalizer::provider::NormalizerNfdDataV1,
    icu_normalizer::provider::Baked,
    false
);
delegate!(
    icu_normalizer::provider::NormalizerNfdTablesV1,
    icu_normalizer::provider::Baked,
    false
);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Options {
    sensitivity: u32,
    case_first: u32,
    numeric: u32,
    ignore_punctuation: u32,
}
pub struct Handle(Collator);

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
fn preferences(locale: Locale, usage: u32, name: &[u8]) -> Result<CollatorPreferences, i32> {
    if usage > 1 || (usage == 1 && !name.is_empty()) {
        return Err(RANGE);
    }
    let german = locale.id.language.as_str() == "de";
    let mut prefs: CollatorPreferences = locale.into();
    // The semantic default ignores an incoming co extension. The stock bake
    // omits CLDR search data. Our explicit search policy uses German phonebook
    // (the German-specific component of CLDR search), locale default elsewhere.
    prefs.collation_type = if !name.is_empty() {
        let text = std::str::from_utf8(name).map_err(|_| RANGE)?;
        if !types()?.contains(text) {
            return Err(UNSUPPORTED);
        }
        Some(
            CollationType::try_from(&Value::try_from_utf8(name).map_err(|_| RANGE)?)
                .map_err(|_| RANGE)?,
        )
    } else if usage == 1 && german {
        Some(CollationType::Phonebk)
    } else {
        None
    };
    Ok(prefs)
}
fn construct(prefs: CollatorPreferences, options: CollatorOptions) -> Result<Collator, i32> {
    let expected = prefs
        .collation_type
        .as_ref()
        .map(|co| co.as_str())
        .unwrap_or("");
    Collator::try_new_unstable(&Strict { expected }, prefs, options).map_err(|_| {
        if expected.is_empty() {
            BACKEND
        } else {
            UNSUPPORTED
        }
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_collation_types(
    opaque: *mut c_void,
    emit: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> i32,
) -> i32 {
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        for value in types()? {
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
pub unsafe extern "C" fn qjs_icu4x_collation_supported(
    locale: *const u8,
    locale_length: usize,
    name: *const u8,
    name_length: usize,
    out: *mut u32,
) -> i32 {
    unsafe {
        write_result(out, || {
            let locale = Locale::try_from_utf8(input(locale, locale_length)?).map_err(|_| RANGE)?;
            let name = input(name, name_length)?;
            let Ok(text) = std::str::from_utf8(name) else {
                return Ok(0);
            };
            if !types()?.contains(text) {
                return Ok(0);
            }
            let prefs = preferences(locale, 0, name)?;
            let locale = CollationMetadataV1::make_locale(prefs.locale_preferences);
            let attribute = icu_provider::DataMarkerAttributes::from_str_or_panic(text);
            let request = DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(attribute, &locale),
                ..Default::default()
            };
            Ok(u32::from(
                <Baked as DataProvider<CollationMetadataV1>>::load(&Baked, request).is_ok(),
            ))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_collation_defaults(
    locale: *const u8,
    length: usize,
    usage: u32,
    out: *mut Options,
) -> i32 {
    unsafe {
        write_result(out, || {
            let locale = Locale::try_from_utf8(input(locale, length)?).map_err(|_| RANGE)?;
            let handle = construct(preferences(locale, usage, &[])?, Default::default())?;
            let resolved = handle.as_borrowed().resolved_options();
            let sensitivity = if usage == 1 {
                match (resolved.strength, resolved.case_level) {
                    (Strength::Primary, CaseLevel::Off) => 1,
                    (Strength::Primary, CaseLevel::On) => 3,
                    (Strength::Secondary, CaseLevel::Off) => 2,
                    _ => 0,
                }
            } else {
                0
            };
            Ok(Options {
                sensitivity,
                case_first: match resolved.case_first {
                    CollationCaseFirst::Upper => 1,
                    CollationCaseFirst::Lower => 2,
                    _ => 0,
                },
                numeric: u32::from(resolved.numeric == CollationNumericOrdering::True),
                ignore_punctuation: u32::from(
                    resolved.alternate_handling == AlternateHandling::Shifted,
                ),
            })
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_collator_open(
    locale: *const u8,
    length: usize,
    usage: u32,
    name: *const u8,
    name_length: usize,
    options: Options,
    out: *mut *mut Handle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if options.sensitivity > 3
                || options.case_first > 2
                || options.numeric > 1
                || options.ignore_punctuation > 1
            {
                return Err(RANGE);
            }
            let locale = Locale::try_from_utf8(input(locale, length)?).map_err(|_| RANGE)?;
            let mut prefs = preferences(locale, usage, input(name, name_length)?)?;
            prefs.case_first = Some(match options.case_first {
                1 => CollationCaseFirst::Upper,
                2 => CollationCaseFirst::Lower,
                _ => CollationCaseFirst::False,
            });
            prefs.numeric_ordering = Some(if options.numeric != 0 {
                CollationNumericOrdering::True
            } else {
                CollationNumericOrdering::False
            });
            let mut resolved = CollatorOptions::default();
            resolved.strength = Some(match options.sensitivity {
                0 => Strength::Tertiary,
                2 => Strength::Secondary,
                _ => Strength::Primary,
            });
            resolved.case_level = Some(if options.sensitivity == 3 {
                CaseLevel::On
            } else {
                CaseLevel::Off
            });
            resolved.alternate_handling = Some(if options.ignore_punctuation != 0 {
                AlternateHandling::Shifted
            } else {
                AlternateHandling::NonIgnorable
            });
            resolved.max_variable = Some(MaxVariable::Punctuation);
            Ok(Box::into_raw(Box::new(Handle(construct(prefs, resolved)?))))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_collator_close(handle: *mut Handle) {
    if !handle.is_null() {
        unsafe { drop(Box::from_raw(handle)) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_collator_compare(
    handle: *const Handle,
    left: *const u16,
    left_length: usize,
    right: *const u16,
    right_length: usize,
    out: *mut i32,
) -> i32 {
    unsafe {
        write_result(out, || {
            unsafe fn units<'a>(data: *const u16, length: usize) -> Result<&'a [u16], i32> {
                if length > isize::MAX as usize / 2 || (length != 0 && data.is_null()) {
                    return Err(RANGE);
                }
                Ok(if length == 0 {
                    &[]
                } else {
                    unsafe { std::slice::from_raw_parts(data, length) }
                })
            }
            let handle = handle.as_ref().ok_or(RANGE)?;
            let order = handle
                .0
                .as_borrowed()
                .compare_utf16(units(left, left_length)?, units(right, right_length)?);
            Ok(match order {
                std::cmp::Ordering::Less => -1,
                std::cmp::Ordering::Equal => 0,
                std::cmp::Ordering::Greater => 1,
            })
        })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_collation_check_version(data: *const u8, length: usize) -> i32 {
    match unsafe { input(data, length) } {
        Ok(value) if value == icu_collator_data::cldr_tag!().as_bytes() => 0,
        Ok(_) => BACKEND,
        Err(status) => status,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn named_inventory_never_relies_on_constructor_fallback() {
        use icu_provider::IterableDataProvider;
        let identifiers =
            <inventory::Provider as IterableDataProvider<CollationMetadataV1>>::iter_ids(
                &inventory::Provider,
            )
            .unwrap();
        for id in identifiers {
            let name = id.marker_attributes.as_str();
            if name.is_empty() || name == "search" || name == "standard" {
                continue;
            }
            let locale = Locale::try_from_str(&id.locale.to_string()).unwrap();
            // Every advertised named payload must construct without dropping
            // its co attribute, including root-inherited named tailorings.
            let prefs = preferences(locale, 0, name.as_bytes()).unwrap();
            construct(prefs, Default::default()).unwrap();
        }
        let locale = Locale::try_from_str("en").unwrap();
        assert!(
            construct(
                preferences(locale, 0, b"phonebk").unwrap(),
                Default::default()
            )
            .is_err()
        );
    }

    #[test]
    fn stock_search_and_options() {
        let locale = Locale::try_from_str("de").unwrap();
        let search = construct(
            preferences(locale.clone(), 1, &[]).unwrap(),
            Default::default(),
        )
        .unwrap();
        let sort = construct(preferences(locale, 0, &[]).unwrap(), Default::default()).unwrap();
        assert_eq!(
            search.as_borrowed().compare("AE", "Ä"),
            std::cmp::Ordering::Less
        );
        assert_eq!(
            sort.as_borrowed().compare("AE", "Ä"),
            std::cmp::Ordering::Greater
        );
    }
}
