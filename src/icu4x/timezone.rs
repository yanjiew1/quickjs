/* Private ICU4X timezone identity and Windows mapping bridge.
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
use icu_locale_core::subtags::Region;
use icu_provider::DataProvider;
use icu_time::zone::{WindowsParser, iana::IanaParserExtended};
use std::ffi::c_char;

#[repr(C)]
pub struct ZoneName {
    name: [c_char; 256],
}

fn name(value: &str) -> Result<ZoneName, i32> {
    if value.is_empty() || value.len() >= 256 || !value.is_ascii() {
        return Err(BACKEND);
    }
    let mut output = ZoneName { name: [0; 256] };
    for (destination, source) in output.name.iter_mut().zip(value.as_bytes()) {
        *destination = *source as c_char;
    }
    Ok(output)
}

unsafe fn bytes<'a>(input: *const u8, length: usize, maximum: usize) -> Result<&'a [u8], i32> {
    if length > maximum || (length != 0 && input.is_null()) {
        return Err(RANGE);
    }
    if length == 0 {
        return Ok(&[]);
    }
    // SAFETY: the caller supplies the complete readable span. In particular,
    // do not construct even an empty Rust slice from a null C pointer.
    Ok(unsafe { std::slice::from_raw_parts(input, length) })
}

fn normalized(input: &[u8]) -> Result<ZoneName, i32> {
    let parsed = IanaParserExtended::new().parse_from_utf8(input);
    if parsed.time_zone.is_unknown() {
        return Err(UNSUPPORTED);
    }
    name(parsed.normalized)
}

fn windows(input: &[u8], territory: &[u8]) -> Result<ZoneName, i32> {
    let region = if territory.len() == 2 {
        Region::try_from_utf8(territory).ok()
    } else {
        None
    };
    let parser = WindowsParser::new();
    let mut selected = parser.parse_from_utf8(input, region);
    if selected.is_none() && region.is_some() {
        selected = parser.parse_from_utf8(input, None);
    }
    if selected.is_none() && input.is_ascii() {
        // OS keys have canonical case. Preserve the C helper's inexpensive
        // compatibility policy for other callers through a bounded fallback
        // over the same ICU4X payload, without shipping a second CLDR table.
        let response = <icu_time::provider::Baked as DataProvider<
            icu_time::provider::windows::TimezoneIdentifiersWindowsV1,
        >>::load(&icu_time::provider::Baked, Default::default())
        .map_err(|_| BACKEND)?;
        for (pass, wanted) in [region, None].into_iter().enumerate() {
            if pass == 1 && region.is_none() {
                break;
            }
            let region = wanted.as_ref().map(|value| value.as_str()).unwrap_or("001");
            for (key, index) in response.payload.get().map.iter() {
                if let Some((key, country)) = key.rsplit_once('/') {
                    if key.as_bytes().eq_ignore_ascii_case(input) && country == region {
                        selected = Some(
                            response
                                .payload
                                .get()
                                .bcp47_ids
                                .get(index as usize)
                                .ok_or(BACKEND)?,
                        );
                        break;
                    }
                }
            }
            if selected.is_some() {
                break;
            }
        }
    }
    let selected = selected.ok_or(UNSUPPORTED)?;
    // This release preserves territorial identities in its BCP47 IDs. The C
    // rules companion still owns ECMA primary normalization and final admission.
    IanaParserExtended::new()
        .iter()
        .find(|entry| entry.time_zone == selected)
        .map(|entry| name(entry.canonical))
        .unwrap_or(Err(BACKEND))
}

/// # Safety
/// Input spans must be readable for their lengths; output is writable/aligned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_timezone_normalize(
    input: *const u8,
    length: usize,
    output: *mut ZoneName,
) -> i32 {
    unsafe { write_result(output, || normalized(bytes(input, length, 255)?)) }
}

/// # Safety
/// Input spans must be readable for their lengths; output is writable/aligned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_windows_timezone(
    input: *const u8,
    length: usize,
    territory: *const u8,
    territory_length: usize,
    output: *mut ZoneName,
) -> i32 {
    unsafe {
        write_result(output, || {
            windows(
                bytes(input, length, 127)?,
                bytes(territory, territory_length, 3)?,
            )
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn text(value: ZoneName) -> String {
        String::from_utf8(
            value
                .name
                .iter()
                .take_while(|v| **v != 0)
                .map(|v| *v as u8)
                .collect(),
        )
        .unwrap()
    }
    #[test]
    fn normalized_aliases_retain_spelling() {
        assert_eq!(text(normalized(b"Asia/CALCUTTA").unwrap()), "Asia/Calcutta");
        assert_eq!(
            text(normalized(b"europe/bratislava").unwrap()),
            "Europe/Bratislava"
        );
        assert_eq!(normalized(b"Etc/Unknown").err(), Some(UNSUPPORTED));
        assert_eq!(normalized(b"not/a/zone").err(), Some(UNSUPPORTED));
    }
    #[test]
    fn windows_country_and_world_fallback() {
        assert_eq!(
            text(windows(b"Central Europe Standard Time", b"SK").unwrap()),
            "Europe/Bratislava"
        );
        assert_eq!(
            text(windows(b"Central Europe Standard Time", b"CZ").unwrap()),
            "Europe/Prague"
        );
        assert_eq!(
            text(windows(b"Pacific Standard Time", b"TW").unwrap()),
            "America/Los_Angeles"
        );
        assert_eq!(
            text(windows(b"pacific standard time", b"US").unwrap()),
            "America/Los_Angeles"
        );
        assert_eq!(windows(b"unknown", b"US").err(), Some(UNSUPPORTED));
    }
    #[test]
    fn invalid_input_preserves_output() {
        let mut output = ZoneName { name: [7; 256] };
        assert_eq!(
            unsafe { qjs_icu4x_timezone_normalize(std::ptr::null(), 1, &mut output) },
            RANGE
        );
        assert_eq!(output.name, [7; 256]);
        assert_eq!(
            unsafe { qjs_icu4x_timezone_normalize(std::ptr::null(), 0, &mut output) },
            UNSUPPORTED
        );
        assert_eq!(output.name, [7; 256]);
    }
}
