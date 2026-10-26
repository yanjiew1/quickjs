/* Private ICU4X source-specific unit patterns.
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
use crate::pattern::{SinglePattern, single_pattern};
use crate::plural_selector::selector;
use crate::{BACKEND, RANGE, UNSUPPORTED, write_result};
use icu_experimental::dimension::provider::units::{
    categorized_display_names::*, display_names::UnitsDisplayNames,
};
use icu_locale_core::Locale;
use icu_provider::{
    DataErrorKind, DataIdentifierBorrowed, DataMarkerAttributes, DataPayload, DataProvider,
    DataRequest, marker::DataMarkerExt,
};

mod source {
    extern crate alloc;
    use icu_experimental_data::*;
    pub mod icu {
        pub use icu_experimental as experimental;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_units_names_area_core_v1!(Provider);
    impl_units_names_area_extended_v1!(Provider);
    impl_units_names_area_outlier_v1!(Provider);
    impl_units_names_duration_core_v1!(Provider);
    impl_units_names_duration_extended_v1!(Provider);
    impl_units_names_duration_outlier_v1!(Provider);
    impl_units_names_length_core_v1!(Provider);
    impl_units_names_length_extended_v1!(Provider);
    impl_units_names_length_outlier_v1!(Provider);
    impl_units_names_mass_core_v1!(Provider);
    impl_units_names_mass_extended_v1!(Provider);
    impl_units_names_mass_outlier_v1!(Provider);
    impl_units_names_volume_core_v1!(Provider);
    impl_units_names_volume_extended_v1!(Provider);
    impl_units_names_volume_outlier_v1!(Provider);
}

pub struct Handle(DataPayload<UnitsNamesAreaCoreV1>);

unsafe fn input<'a>(data: *const u8, length: usize) -> Result<&'a [u8], i32> {
    if data.is_null() || length == 0 || length > isize::MAX as usize {
        return Err(RANGE);
    }
    Ok(unsafe { std::slice::from_raw_parts(data, length) })
}

// Tier lookup admits only a payload at the already selected source identity.
fn load_exact<M>(
    locale: &Locale,
    attributes: &DataMarkerAttributes,
) -> Result<Option<DataPayload<UnitsNamesAreaCoreV1>>, i32>
where
    M: icu_provider::DataMarker<DataStruct = UnitsDisplayNames<'static>>,
    source::Provider: DataProvider<M>,
{
    let source_locale = M::make_locale(locale.into());
    let mut request = DataRequest {
        id: DataIdentifierBorrowed::for_marker_attributes_and_locale(attributes, &source_locale),
        ..Default::default()
    };
    // Missing tiers are normal speculative requests, not user diagnostics.
    request.metadata.silent = true;
    match <source::Provider as DataProvider<M>>::load(&source::Provider, request) {
        Ok(response)
            if response
                .metadata
                .locale
                .as_ref()
                .is_none_or(|actual| actual == &source_locale) =>
        {
            Ok(Some(response.payload.cast()))
        }
        Ok(_) => Ok(None),
        Err(error) if error.kind == DataErrorKind::IdentifierNotFound => Ok(None),
        Err(_) => Err(BACKEND),
    }
}

macro_rules! tiers {
    ($locale:expr, $attributes:expr, $($marker:ty),+ $(,)?) => {{
        $(
            if let Some(payload) = load_exact::<$marker>($locale, $attributes)? {
                return Ok(Box::into_raw(Box::new(Handle(payload))));
            }
        )+
        Err(UNSUPPORTED)
    }};
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_unit_open(
    locale: *const u8,
    length: usize,
    group: *const u8,
    group_length: usize,
    width: u32,
    out: *mut *mut Handle,
) -> i32 {
    unsafe {
        write_result(out, || {
            let locale = input(locale, length)?;
            let locale = if locale == b"root" {
                Locale::try_from_utf8(b"und").map_err(|_| RANGE)?
            } else {
                Locale::try_from_utf8(locale).map_err(|_| RANGE)?
            };
            let group = std::str::from_utf8(input(group, group_length)?).map_err(|_| RANGE)?;
            if !group
                .bytes()
                .all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'-')
                || group.split('-').any(str::is_empty)
            {
                return Err(RANGE);
            }
            let (family, unit) = group.split_once('-').ok_or(RANGE)?;
            let width = match width {
                0 => "long",
                1 => "short",
                2 => "narrow",
                _ => return Err(RANGE),
            };
            let attribute = format!("{width}-{unit}");
            let attributes = DataMarkerAttributes::try_from_str(&attribute).map_err(|_| RANGE)?;
            // Core/Extended/Outlier partition locale/unit records. A convenience
            // constructor's tier order is not a width/source fallback policy.
            // Try every tier, accepting only the requested source identity. The C
            // source layers already chose the actual leaf origin; inherited public
            // payloads cannot establish that leaf's provenance.
            match family {
                "area" => tiers!(
                    &locale,
                    attributes,
                    UnitsNamesAreaCoreV1,
                    UnitsNamesAreaExtendedV1,
                    UnitsNamesAreaOutlierV1
                ),
                "duration" => tiers!(
                    &locale,
                    attributes,
                    UnitsNamesDurationCoreV1,
                    UnitsNamesDurationExtendedV1,
                    UnitsNamesDurationOutlierV1
                ),
                "length" => tiers!(
                    &locale,
                    attributes,
                    UnitsNamesLengthCoreV1,
                    UnitsNamesLengthExtendedV1,
                    UnitsNamesLengthOutlierV1
                ),
                "mass" => tiers!(
                    &locale,
                    attributes,
                    UnitsNamesMassCoreV1,
                    UnitsNamesMassExtendedV1,
                    UnitsNamesMassOutlierV1
                ),
                "volume" => tiers!(
                    &locale,
                    attributes,
                    UnitsNamesVolumeCoreV1,
                    UnitsNamesVolumeExtendedV1,
                    UnitsNamesVolumeOutlierV1
                ),
                _ => Err(UNSUPPORTED),
            }
        })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_unit_close(handle: *mut Handle) {
    if !handle.is_null() {
        unsafe { drop(Box::from_raw(handle)) };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_unit_pattern(
    handle: *const Handle,
    category: u32,
    out: *mut SinglePattern,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            let rules = selector(category)?;
            // Source-case and numeric exact0/1 forms stay in the C companion when
            // this public payload cannot represent their individual identities.
            single_pattern(handle.0.get().get(2_u32.into(), &rules))
        })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_unit_check_version(data: *const u8, length: usize) -> i32 {
    match unsafe { input(data, length) } {
        Ok(value) if value == icu_experimental_data::cldr_tag!().as_bytes() => 0,
        Ok(_) => BACKEND,
        Err(status) => status,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use icu_provider::DataMarker;

    fn origin<M: DataMarker>(locale: &str, attribute: &str) -> String
    where
        source::Provider: DataProvider<M>,
    {
        let locale = Locale::try_from_str(locale).unwrap();
        let source = M::make_locale((&locale).into());
        let attributes = DataMarkerAttributes::try_from_str(attribute).unwrap();
        let response = <source::Provider as DataProvider<M>>::load(
            &source::Provider,
            DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(attributes, &source),
                ..Default::default()
            },
        );
        match response {
            Ok(data) => data
                .metadata
                .locale
                .map(|v| v.to_string())
                .unwrap_or("exact".into()),
            Err(_) => "missing".into(),
        }
    }
    #[test]
    fn exact_later_tier_wins_over_inherited_core() {
        assert_eq!(
            origin::<UnitsNamesLengthCoreV1>("de", "long-kilometer"),
            "und"
        );
        assert_eq!(
            origin::<UnitsNamesLengthExtendedV1>("de", "long-kilometer"),
            "exact"
        );
        unsafe {
            let mut handle = std::ptr::null_mut();
            assert_eq!(
                qjs_icu4x_unit_open(
                    b"de".as_ptr(),
                    2,
                    b"length-kilometer".as_ptr(),
                    16,
                    0,
                    &mut handle
                ),
                0
            );
            let mut pattern = std::mem::MaybeUninit::uninit();
            assert_eq!(qjs_icu4x_unit_pattern(handle, 5, pattern.as_mut_ptr()), 0);
            let pattern = pattern.assume_init();
            assert_eq!(
                std::slice::from_raw_parts(pattern.suffix.data, pattern.suffix.length),
                b" Kilometer"
            );
            qjs_icu4x_unit_close(handle);
        }
    }
    #[test]
    fn parent_only_payload_does_not_claim_exact_source() {
        assert_eq!(origin::<UnitsNamesLengthCoreV1>("en-GB", "long-inch"), "en");
        assert_eq!(
            origin::<UnitsNamesLengthExtendedV1>("en-GB", "long-inch"),
            "en-001"
        );
        unsafe {
            let mut handle = std::ptr::null_mut();
            assert_eq!(
                qjs_icu4x_unit_open(
                    b"en-GB".as_ptr(),
                    5,
                    b"length-inch".as_ptr(),
                    11,
                    0,
                    &mut handle
                ),
                UNSUPPORTED
            );
            assert!(handle.is_null());
        }
    }
}
