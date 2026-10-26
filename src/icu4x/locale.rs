/* Private ICU4X locale identity adapter.
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
use crate::{BACKEND, RANGE, UNSUPPORTED, locale_data::Aliases, write_result};
use icu_locale::{LocaleCanonicalizer, LocaleExpander};
use icu_locale_core::{
    Locale,
    extensions::unicode::{Attributes, Key, Value},
    subtags::Subtag,
};
use std::{
    borrow::Cow,
    collections::BTreeMap,
    ffi::c_void,
    panic::{AssertUnwindSafe, catch_unwind},
};

pub struct LocaleHandle {
    data: Aliases,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct ByteSink {
    opaque: *mut c_void,
    write: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> i32,
}
#[repr(C)]
pub struct Bytes {
    pub data: *const u8,
    pub length: usize,
}

unsafe fn text<'a>(data: *const u8, length: usize) -> Result<&'a str, i32> {
    if length > isize::MAX as usize || (length != 0 && data.is_null()) {
        return Err(RANGE);
    }
    if length == 0 {
        return Ok("");
    }
    let bytes = unsafe { std::slice::from_raw_parts(data, length) };
    if !bytes.is_ascii() || bytes.contains(&0) {
        return Err(RANGE);
    }
    std::str::from_utf8(bytes).map_err(|_| RANGE)
}

struct Frame<'a> {
    main: Cow<'a, str>,
    unicode: Option<String>,
    transform: Option<String>,
}
fn frame(input: &str) -> Result<Frame<'_>, i32> {
    // The public parser removes embedded true subtags. Isolate only u/t;
    // let ICU parse/canonicalize the base and all other extension kinds.
    if !input
        .as_bytes()
        .windows(3)
        .any(|w| w[0] == b'-' && w[2] == b'-' && matches!(w[1], b'u' | b'U' | b't' | b'T'))
    {
        return Ok(Frame {
            main: Cow::Borrowed(input),
            unicode: None,
            transform: None,
        });
    }
    let parts: Vec<_> = input.split('-').collect();
    let mut position = 1;
    while position < parts.len() && parts[position].len() != 1 {
        position += 1;
    }
    let mut result = Frame {
        main: Cow::Owned(parts[..position].join("-")),
        unicode: None,
        transform: None,
    };
    while position < parts.len() {
        let singleton = parts[position].to_ascii_lowercase();
        let start = position + 1;
        position = start;
        if singleton == "x" {
            position = parts.len();
        } else {
            while position < parts.len() && parts[position].len() != 1 {
                position += 1;
            }
        }
        let body = parts[start..position].join("-");
        let slot = if singleton == "u" {
            Some(&mut result.unicode)
        } else if singleton == "t" {
            Some(&mut result.transform)
        } else {
            None
        };
        if let Some(slot) = slot {
            if slot.is_some() || body.is_empty() {
                return Err(RANGE);
            }
            *slot = Some(body.to_ascii_lowercase());
        } else {
            result.main.to_mut().push('-');
            result.main.to_mut().push_str(&singleton);
            result.main.to_mut().push('-');
            result.main.to_mut().push_str(&body);
        }
    }
    Ok(result)
}
fn is_key(part: &str, transformed: bool) -> bool {
    let b = part.as_bytes();
    b.len() == 2
        && if transformed {
            b[0].is_ascii_lowercase() && b[1].is_ascii_digit()
        } else {
            b[0].is_ascii_alphanumeric() && b[1].is_ascii_lowercase()
        }
}
fn subtag(part: &str) -> bool {
    (3..=8).contains(&part.len()) && part.bytes().all(|c| c.is_ascii_alphanumeric())
}
fn fields<'a>(
    parts: &[&'a str],
    mut position: usize,
    transformed: bool,
) -> Result<BTreeMap<&'a str, String>, i32> {
    let mut result = BTreeMap::new();
    while position < parts.len() {
        let key = parts[position];
        if !is_key(key, transformed) {
            return Err(RANGE);
        }
        position += 1;
        let start = position;
        while position < parts.len() && !is_key(parts[position], transformed) {
            if !subtag(parts[position]) {
                return Err(RANGE);
            }
            position += 1;
        }
        if transformed && start == position {
            return Err(RANGE);
        }
        result
            .entry(key)
            .or_insert_with(|| parts[start..position].join("-"));
    }
    Ok(result)
}

fn canonical(handle: &LocaleHandle, input: &str, operation: u32) -> Result<String, i32> {
    if operation > 2 {
        return Err(RANGE);
    }
    let frame = frame(input)?;
    let input = crate::language_input::LanguageInput::new(&frame.main);
    let mut locale = Locale::try_from_str(input.as_str()).map_err(|_| RANGE)?;
    let data = unsafe { handle.data.view()? };
    if let Some(raw) = frame.unicode {
        let parts: Vec<_> = raw.split('-').collect();
        let position = parts
            .iter()
            .position(|part| is_key(part, false))
            .unwrap_or(parts.len());
        if parts[..position].iter().any(|part| !subtag(part)) {
            return Err(RANGE);
        }
        locale.extensions.unicode.attributes =
            Attributes::try_from_str(&parts[..position].join("-")).map_err(|_| RANGE)?;
        for (key, value) in fields(&parts, position, false)? {
            let value = data.normalize(b'u', key, &value)?;
            // Only a whole uvalue true disappears. FromIterator is public and
            // preserves embedded true; try_from_str would silently remove it.
            let value = if value == "true" { "" } else { value.as_ref() };
            let subtags: Result<Vec<_>, _> = value
                .split('-')
                .filter(|s| !s.is_empty())
                .map(Subtag::try_from_str)
                .collect();
            let value: Value = subtags.map_err(|_| RANGE)?.into_iter().collect();
            locale.extensions.unicode.keywords.set(
                Key::try_from_str(data.key_name(b'u', key)?).map_err(|_| RANGE)?,
                value,
            );
        }
    }
    LocaleCanonicalizer::new_extended().canonicalize(&mut locale);
    if !input.masked() && (operation == 1 || operation == 2) {
        let expander = LocaleExpander::new_extended();
        // Maximize first also strips the standard unknown placeholders Zzzz
        // and ZZ. ICU4X 2.4 then excludes generic und fallback: retain
        // our optional result using its own public likely tuple, without
        // overwriting genuinely unknown explicit scripts or regions.
        expander.maximize(&mut locale.id);
        if locale.id.language.is_unknown() {
            use icu_locale_fallback::provider::{Baked, LocaleLikelySubtagsLanguageV1};
            use icu_provider::DataProvider;
            let response = <Baked as DataProvider<LocaleLikelySubtagsLanguageV1>>::load(
                &Baked,
                Default::default(),
            )
            .map_err(|_| BACKEND)?;
            let (language, script, region) = response.payload.get().und;
            locale.id.language = language;
            locale.id.script.get_or_insert(script);
            locale.id.region.get_or_insert(region);
        }
        if operation == 2 {
            expander.minimize(&mut locale.id);
        }
    }
    let mut output = input.restore(locale.to_string());
    if let Some(raw) = frame.transform {
        let parts: Vec<_> = raw.split('-').collect();
        let position = parts
            .iter()
            .position(|part| is_key(part, true))
            .unwrap_or(parts.len());
        let mut body = String::new();
        if position != 0 {
            let tag = parts[..position].join("-");
            let input = crate::language_input::LanguageInput::new(&tag);
            let mut language = Locale::try_from_str(input.as_str()).map_err(|_| RANGE)?;
            if !language.extensions.is_empty() {
                return Err(RANGE);
            }
            LocaleCanonicalizer::new_extended().canonicalize(&mut language);
            body = input.restore(language.id.to_string()).to_ascii_lowercase();
        }
        for (key, value) in fields(&parts, position, true)? {
            if !body.is_empty() {
                body.push('-');
            }
            body.push_str(data.key_name(b't', key)?);
            body.push('-');
            body.push_str(&data.normalize(b't', key, &value)?);
        }
        // t precedes u and larger singleton names; private use stays last.
        let mut insertion = output.len();
        let mut at = 0;
        for part in output.split('-') {
            if part.len() == 1 && part.as_bytes()[0] >= b't' {
                insertion = at - 1;
                break;
            }
            at += part.len() + 1;
        }
        output.insert_str(insertion, &format!("-t-{body}"));
    }
    Ok(output)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_locale_open(
    data: *const Aliases,
    out: *mut *mut LocaleHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if data.is_null() {
                return Err(RANGE);
            }
            let data = *data;
            data.validate()?;
            Ok(Box::into_raw(Box::new(LocaleHandle { data })))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_locale_close(handle: *mut LocaleHandle) {
    if !handle.is_null() {
        unsafe {
            drop(Box::from_raw(handle));
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_locale_identity(
    handle: *const LocaleHandle,
    input: *const u8,
    length: usize,
    operation: u32,
    sink: ByteSink,
) -> i32 {
    match catch_unwind(AssertUnwindSafe(|| {
        if handle.is_null() {
            return Err(RANGE);
        }
        let output = canonical(
            unsafe { &*handle },
            unsafe { text(input, length)? },
            operation,
        )?;
        let status = unsafe { (sink.write)(sink.opaque, output.as_ptr(), output.len()) };
        if status == 0 { Ok(()) } else { Err(status) }
    })) {
        Ok(Ok(())) => 0,
        Ok(Err(status)) => status,
        Err(_) => BACKEND,
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_locale_source_type(
    handle: *const LocaleHandle,
    key: *const u8,
    key_length: usize,
    value: *const u8,
    value_length: usize,
    out: *mut Bytes,
) -> i32 {
    unsafe {
        write_result(out, || {
            if handle.is_null() {
                return Err(RANGE);
            }
            let data = (&*handle).data.view()?;
            let key = text(key, key_length)?;
            let value = text(value, value_length)?;
            let key = data.key(b'u', key)?.ok_or(UNSUPPORTED)?;
            let row = data
                .source_type(data.canonical_key(key)?, value, false)?
                .ok_or(UNSUPPORTED)?;
            let result = data.text(data.types[row].name)?;
            Ok(Bytes {
                data: result.as_ptr(),
                length: result.len(),
            })
        })
    }
}

// Query the exact public script sets. Do not infer a likely script here: the
// common Locale preference algorithm makes that decision before this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_locale_script_direction(
    script: *const u8,
    length: usize,
    out: *mut u32,
) -> i32 {
    unsafe {
        write_result(out, || {
            use icu_locale::provider::{Baked, LocaleScriptDirectionV1};
            use icu_locale_core::subtags::Script;
            use icu_provider::DataProvider;
            let script = Script::try_from_str(text(script, length)?).map_err(|_| RANGE)?;
            let payload =
                <Baked as DataProvider<LocaleScriptDirectionV1>>::load(&Baked, Default::default())
                    .map_err(|_| BACKEND)?
                    .payload;
            let key = script.to_tinystr().to_unvalidated();
            if payload.get().rtl.binary_search(&key).is_ok() {
                Ok(2)
            } else if payload.get().ltr.binary_search(&key).is_ok() {
                Ok(1)
            } else {
                Err(UNSUPPORTED)
            }
        })
    }
}
