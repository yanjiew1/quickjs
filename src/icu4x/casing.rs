/* Private ICU4X locale case mapping.
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
use crate::{
    BACKEND, MEMORY, RANGE,
    text::{TextOutput, TextSink, Utf16},
};
use icu_casemap::CaseMapper;
use icu_locale_core::LanguageIdentifier;
use std::panic::{AssertUnwindSafe, catch_unwind};
use writeable::Writeable;

fn mapped_run(
    input: &[u16],
    language: &LanguageIdentifier,
    lower: bool,
    output: &mut TextOutput,
) -> Result<(), i32> {
    if input.is_empty() {
        return Ok(());
    }
    let mut scalar_text = String::new();
    // Three UTF-8 bytes per code unit bounds even a supplementary pair.
    // Reserve fallibly before push so our conversion never aborts on OOM.
    scalar_text
        .try_reserve_exact(input.len().checked_mul(3).ok_or(MEMORY)?)
        .map_err(|_| MEMORY)?;
    for scalar in char::decode_utf16(input.iter().copied()) {
        scalar_text.push(scalar.map_err(|_| BACKEND)?);
    }
    let mapper = CaseMapper::new();
    let result = if lower {
        mapper.lowercase(&scalar_text, language).write_to(output)
    } else {
        mapper.uppercase(&scalar_text, language).write_to(output)
    };
    result.map_err(|_| {
        if output.error == 0 {
            BACKEND
        } else {
            output.error
        }
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_case_map(
    locale: *const u8,
    locale_length: usize,
    input: *const u16,
    length: usize,
    lower: u32,
    sink: TextSink,
) -> i32 {
    match catch_unwind(AssertUnwindSafe(|| {
        if lower > 1 || locale.is_null() || locale_length == 0 || locale_length > 8 {
            return Err(RANGE);
        }
        let language = LanguageIdentifier::try_from_utf8(unsafe {
            std::slice::from_raw_parts(locale, locale_length)
        })
        .map_err(|_| RANGE)?;
        let units = unsafe {
            Utf16 {
                data: input,
                length,
            }
            .units()?
        };
        let mut output = TextOutput::new(sink);
        let mut start = 0;
        let mut position = 0;
        while position < units.len() {
            let unit = units[position];
            if (0xd800..=0xdbff).contains(&unit)
                && position + 1 < units.len()
                && (0xdc00..=0xdfff).contains(&units[position + 1])
            {
                position += 2;
                continue;
            }
            if (0xd800..=0xdfff).contains(&unit) {
                // A lone surrogate is uncased, non-case-ignorable and CCC=0.
                // It ends casing context in both directions. Map complete
                // scalar runs and preserve the original unit at that boundary.
                mapped_run(&units[start..position], &language, lower != 0, &mut output)?;
                output
                    .units(&units[position..position + 1])
                    .map_err(|_| output.error)?;
                start = position + 1;
            }
            position += 1;
        }
        mapped_run(&units[start..], &language, lower != 0, &mut output)?;
        output.flush().map_err(|_| output.error)
    })) {
        Ok(Ok(())) => 0,
        Ok(Err(error)) => error,
        Err(_) => BACKEND,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::c_void;
    unsafe extern "C" fn write(opaque: *mut c_void, text: *const u16, length: usize) -> i32 {
        unsafe { &mut *opaque.cast::<Vec<u16>>() }
            .extend_from_slice(unsafe { std::slice::from_raw_parts(text, length) });
        0
    }
    fn map(language: &str, input: &[u16], lower: bool) -> Vec<u16> {
        let mut output = Vec::new();
        let sink = TextSink {
            opaque: (&mut output as *mut Vec<u16>).cast(),
            write,
        };
        assert_eq!(
            unsafe {
                qjs_icu4x_case_map(
                    language.as_ptr(),
                    language.len(),
                    input.as_ptr(),
                    input.len(),
                    u32::from(lower),
                    sink,
                )
            },
            0
        );
        output
    }
    #[test]
    fn buffered_output_and_callback_failure() {
        struct Output {
            text: Vec<u16>,
            calls: usize,
            fail: bool,
        }
        unsafe extern "C" fn collect(opaque: *mut c_void, text: *const u16, length: usize) -> i32 {
            let output = unsafe { &mut *opaque.cast::<Output>() };
            output.calls += 1;
            if output.fail {
                return MEMORY;
            }
            output
                .text
                .extend_from_slice(unsafe { std::slice::from_raw_parts(text, length) });
            0
        }
        let mut input = vec![b'A' as u16; 300];
        input.push(0xd800);
        input.push(b'B' as u16);
        for fail in [false, true] {
            let mut output = Output {
                text: Vec::new(),
                calls: 0,
                fail,
            };
            let sink = TextSink {
                opaque: (&mut output as *mut Output).cast(),
                write: collect,
            };
            let status = unsafe {
                qjs_icu4x_case_map(b"en".as_ptr(), 2, input.as_ptr(), input.len(), 1, sink)
            };
            if fail {
                assert_eq!(status, MEMORY);
                assert_eq!(output.calls, 1);
            } else {
                assert_eq!(status, 0);
                assert_eq!(output.calls, 3);
                assert_eq!(&output.text[..300], &vec![b'a' as u16; 300]);
                assert_eq!(&output.text[300..], &[0xd800, b'b' as u16]);
            }
        }
    }

    #[test]
    fn locale_and_surrogate_contexts() {
        for (language, input, expected, lower) in [
            ("tr", "I\u{323}\u{307}", "i\u{323}", true),
            ("lt", "I\u{301}", "i\u{307}\u{301}", true),
            ("lt", "i\u{307}", "I", false),
            ("und", "ΟΣ", "ος", true),
            ("el", "άι", "ΑΪ", false),
            ("und", "ß\u{10428}", "SS\u{10400}", false),
        ] {
            assert_eq!(
                map(language, &input.encode_utf16().collect::<Vec<_>>(), lower),
                expected.encode_utf16().collect::<Vec<_>>()
            );
        }
        assert_eq!(
            map("tr", &[b'I' as u16, 0xd800, 0x307, 0, b'I' as u16], true),
            [0x131, 0xd800, 0x307, 0, 0x131]
        );
        assert_eq!(
            map("und", &[0x391, 0x3a3, 0xdc00, 0x391, 0x3a3], true),
            [0x3b1, 0x3c2, 0xdc00, 0x3b1, 0x3c2]
        );
        assert!(map("tr", &[], true).is_empty());
    }
}
