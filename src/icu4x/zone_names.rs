/* Private ICU4X timezone name payloads.
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
use crate::pattern::Text;
use crate::{BACKEND, RANGE, UNSUPPORTED, write_result};
use icu_datetime::provider::time_zones::*;
use icu_locale_core::Locale;
use icu_provider::{
    DataErrorKind, DataIdentifierBorrowed, DataMarker, DataPayload, DataProvider, DataRequest,
    marker::DataMarkerExt,
};
use icu_time::{
    provider::TimezonePeriodsV1,
    zone::{TimeZoneVariant, ZoneNameTimestamp, iana::IanaParserExtended},
};

mod source {
    extern crate alloc;
    use icu_datetime_data::*;
    pub mod icu {
        pub use icu_datetime as datetime;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_timezone_names_generic_long_v1!(Provider);
    impl_timezone_names_generic_short_v1!(Provider);
    impl_timezone_names_standard_long_v1!(Provider);
    impl_timezone_names_specific_long_v1!(Provider);
    impl_timezone_names_specific_short_v1!(Provider);
}

fn text(value: &str) -> Text {
    Text {
        data: value.as_ptr(),
        length: value.len(),
    }
}

pub struct Handle {
    generic_long: Option<DataPayload<TimezoneNamesGenericLongV1>>,
    generic_short: Option<DataPayload<TimezoneNamesGenericShortV1>>,
    standard_long: Option<DataPayload<TimezoneNamesStandardLongV1>>,
    specific_long: Option<DataPayload<TimezoneNamesSpecificLongV1>>,
    specific_short: Option<DataPayload<TimezoneNamesSpecificShortV1>>,
    periods: DataPayload<TimezonePeriodsV1>,
}

fn load<M: DataMarker>(
    locale: &Locale,
    checksum: Option<u64>,
) -> Result<Option<DataPayload<M>>, i32>
where
    source::Provider: DataProvider<M>,
{
    let source_locale = M::make_locale(locale.into());
    let mut request = DataRequest {
        id: DataIdentifierBorrowed::for_locale(&source_locale),
        ..Default::default()
    };
    request.metadata.silent = true;
    match <source::Provider as DataProvider<M>>::load(&source::Provider, request) {
        Ok(response)
            if response
                .metadata
                .locale
                .as_ref()
                .is_none_or(|v| v == &source_locale) =>
        {
            if checksum.is_none() || response.metadata.checksum != checksum {
                return Err(BACKEND);
            }
            Ok(Some(response.payload))
        }
        Ok(_) => Ok(None),
        Err(e) if e.kind == DataErrorKind::IdentifierNotFound => Ok(None),
        Err(_) => Err(BACKEND),
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_zone_names_open(
    input: *const u8,
    length: usize,
    out: *mut *mut Handle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if input.is_null() || length == 0 || length > 255 {
                return Err(RANGE);
            }
            let input = std::slice::from_raw_parts(input, length);
            let locale = Locale::try_from_utf8(if input == b"root" { b"und" } else { input })
                .map_err(|_| RANGE)?;
            let periods = <icu_time::provider::Baked as DataProvider<TimezonePeriodsV1>>::load(
                &icu_time::provider::Baked,
                Default::default(),
            )
            .map_err(|_| BACKEND)?;
            let checksum = periods.metadata.checksum;
            let handle = Handle {
                generic_long: load::<TimezoneNamesGenericLongV1>(&locale, checksum)?,
                generic_short: load::<TimezoneNamesGenericShortV1>(&locale, checksum)?,
                standard_long: load::<TimezoneNamesStandardLongV1>(&locale, checksum)?,
                specific_long: load::<TimezoneNamesSpecificLongV1>(&locale, checksum)?,
                specific_short: load::<TimezoneNamesSpecificShortV1>(&locale, checksum)?,
                periods: periods.payload,
            };
            if handle.generic_long.is_none()
                && handle.generic_short.is_none()
                && handle.standard_long.is_none()
                && handle.specific_long.is_none()
                && handle.specific_short.is_none()
            {
                return Err(UNSUPPORTED);
            }
            Ok(Box::into_raw(Box::new(handle)))
        })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_zone_names_close(handle: *mut Handle) {
    if !handle.is_null() {
        drop(unsafe { Box::from_raw(handle) });
    }
}

// A metazone is identified by a C-selected representative zone and an instant
// safely inside its membership. Never reinterpret CLDR indexes as ICU IDs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_zone_names_text(
    handle: *const Handle,
    zone: *const u8,
    length: usize,
    epoch_seconds: i64,
    metazone: u32,
    width: u32,
    kind: u32,
    out: *mut Text,
) -> i32 {
    unsafe {
        write_result(out, || {
            let handle = handle.as_ref().ok_or(RANGE)?;
            if zone.is_null()
                || length == 0
                || length > 255
                || metazone > 1
                || !(1..=2).contains(&width)
                || kind > 2
            {
                return Err(RANGE);
            }
            let zone = std::slice::from_raw_parts(zone, length);
            let id = IanaParserExtended::new().parse_from_utf8(zone).time_zone;
            if id.is_unknown() {
                return Err(UNSUPPORTED);
            }
            let mz = if metazone == 1 {
                Some(
                    handle
                        .periods
                        .get()
                        .get(id, ZoneNameTimestamp::from_epoch_seconds(epoch_seconds))
                        .and_then(|(_, value)| value)
                        .ok_or(UNSUPPORTED)?
                        .id,
                )
            } else {
                None
            };
            let generic = |data: &MetazoneGenericNames<'_>| {
                if let Some(mz) = mz {
                    data.defaults.get(&mz).map(text)
                } else {
                    data.overrides.get(&id).map(text)
                }
            };
            let specific = |data: &MetazoneSpecificNames<'_>, variant| {
                if let Some(mz) = mz {
                    data.defaults.get(&(mz, variant)).map(text)
                } else {
                    data.overrides.get(&(id, variant)).map(text)
                }
            };
            let result = if kind == 0 {
                if width == 1 {
                    handle.generic_long.as_ref().and_then(|p| generic(p.get()))
                } else {
                    handle.generic_short.as_ref().and_then(|p| generic(p.get()))
                }
            } else if width == 1 {
                let specific = handle.specific_long.as_ref().and_then(|p| {
                    specific(
                        p.get(),
                        if kind == 1 {
                            TimeZoneVariant::Standard
                        } else {
                            TimeZoneVariant::Daylight
                        },
                    )
                });
                if kind == 1
                    && specific.is_none()
                    && mz.is_some_and(|mz| {
                        handle
                            .specific_long
                            .as_ref()
                            .is_some_and(|p| p.get().use_standard.iter().any(|v| v == mz))
                    })
                {
                    handle.standard_long.as_ref().and_then(|p| generic(p.get()))
                } else {
                    specific
                }
            } else {
                handle.specific_short.as_ref().and_then(|p| {
                    specific(
                        p.get(),
                        if kind == 1 {
                            TimeZoneVariant::Standard
                        } else {
                            TimeZoneVariant::Daylight
                        },
                    )
                })
            }
            .ok_or(UNSUPPORTED)?;
            Ok(result)
        })
    }
}
