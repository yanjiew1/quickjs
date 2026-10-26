/* Private ICU4X display-name adapter.
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
use crate::text::{TextOutput, TextSink};
use crate::{BACKEND, RANGE, UNSUPPORTED, write_result};
use icu_locale::names::{
    DisplayNamesPreferences, LanguageIdentifierDisplayName, LanguageIdentifierDisplayNameOptions,
    RegionDisplayName, ScriptDisplayName,
};
use icu_locale_core::{
    LanguageIdentifier, Locale,
    subtags::{Region, Script},
};
use icu_provider::DataErrorKind;
use std::{
    fmt,
    panic::{AssertUnwindSafe, catch_unwind},
};
use writeable::{TryWriteable, Writeable};

pub struct NameHandle {
    prefs: DisplayNamesPreferences,
    short: bool,
    dialect: bool,
}
unsafe fn input<'a>(data: *const u8, length: usize) -> Result<&'a [u8], i32> {
    if data.is_null() || length > isize::MAX as usize {
        return Err(RANGE);
    }
    Ok(unsafe { std::slice::from_raw_parts(data, length) })
}
fn data_error(error: icu_provider::DataError) -> i32 {
    if error.kind == DataErrorKind::IdentifierNotFound {
        UNSUPPORTED
    } else {
        BACKEND
    }
}
struct Discard;
impl fmt::Write for Discard {
    fn write_str(&mut self, _: &str) -> fmt::Result {
        Ok(())
    }
}
fn write<T: Writeable>(name: T, sink: TextSink) -> Result<(), i32> {
    let mut output = TextOutput::new(sink);
    if name
        .write_to(&mut output)
        .and_then(|_| output.flush())
        .is_err()
    {
        return Err(if output.error != 0 {
            output.error
        } else {
            BACKEND
        });
    }
    Ok(())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_names_open(
    locale: *const u8,
    length: usize,
    style: u32,
    dialect: u32,
    out: *mut *mut NameHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if style > 2 || dialect > 1 {
                return Err(RANGE);
            }
            let locale = Locale::try_from_utf8(input(locale, length)?).map_err(|_| RANGE)?;
            Ok(Box::into_raw(Box::new(NameHandle {
                prefs: locale.into(),
                short: style != 0,
                dialect: dialect != 0,
            })))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_names_close(handle: *mut NameHandle) {
    if !handle.is_null() {
        unsafe {
            drop(Box::from_raw(handle));
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_names_format(
    handle: *const NameHandle,
    kind: u32,
    code: *const u8,
    length: usize,
    sink: TextSink,
) -> i32 {
    match catch_unwind(AssertUnwindSafe(|| {
        if handle.is_null() {
            return Err(RANGE);
        }
        let handle = unsafe { &*handle };
        let code = unsafe { input(code, length)? };
        match kind {
            0 => {
                use icu_locale::names::LanguageDisplay;
                let language = LanguageIdentifier::try_from_utf8(code).map_err(|_| RANGE)?;
                let mut options = LanguageIdentifierDisplayNameOptions::default();
                options.language_display = Some(if handle.dialect {
                    LanguageDisplay::Dialect
                } else {
                    LanguageDisplay::Standard
                });
                let name = if handle.short {
                    LanguageIdentifierDisplayName::try_new_short_heavy(
                        handle.prefs,
                        language,
                        options,
                    )
                } else {
                    LanguageIdentifierDisplayName::try_new_heavy(handle.prefs, language, options)
                }
                .map_err(data_error)?;
                let name = name.as_borrowed();
                // Absence can be reported by composition after a known prefix.
                // Detect it before writing or enforcing a C output budget;
                // comparing rendered text to its code cannot prove absence.
                if name
                    .try_write_to(&mut Discard)
                    .map_err(|_| BACKEND)?
                    .is_err()
                {
                    return Err(UNSUPPORTED);
                }
                let mut output = TextOutput::new(sink);
                let result = name.try_write_to(&mut output);
                if result.is_err() || output.flush().is_err() {
                    return Err(if output.error != 0 {
                        output.error
                    } else {
                        BACKEND
                    });
                }
                result.map_err(|_| BACKEND)?.map_err(|_| BACKEND)
            }
            1 => {
                let region = Region::try_from_utf8(code).map_err(|_| RANGE)?;
                let name = if handle.short {
                    RegionDisplayName::try_new_short_light(handle.prefs, region)
                } else {
                    RegionDisplayName::try_new_light(handle.prefs, region)
                }
                .map_err(data_error)?;
                write(name, sink)
            }
            2 => {
                let script = Script::try_from_utf8(code).map_err(|_| RANGE)?;
                let name = if handle.short {
                    ScriptDisplayName::try_new_short_heavy(handle.prefs, script)
                } else {
                    ScriptDisplayName::try_new_heavy(handle.prefs, script)
                }
                .map_err(data_error)?;
                write(name, sink)
            }
            #[cfg(feature = "currency_names")]
            3 => {
                let name = crate::currency::plain_name(handle.prefs.locale_preferences, code)?;
                write(&**name.get(), sink)
            }
            _ => Err(RANGE),
        }
    })) {
        Ok(Ok(())) => 0,
        Ok(Err(status)) => status,
        Err(_) => BACKEND,
    }
}
