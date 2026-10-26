/* Private ICU4X locale metadata view.
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
use crate::BACKEND;
use std::borrow::Cow;

// These repr(C) rows mirror intl/icu4x/locale-data.h. The C owner remains
// immutable and alive until the opaque locale handle is closed.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Key {
    pub name: u32,
    pub aliases: u32,
    pub preferred: u32,
    pub extension: u8,
    pub value_type: u8,
    pub deprecated: u8,
    pub reserved: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Type {
    pub key: u16,
    pub deprecated: u8,
    pub variable: u8,
    pub name: u32,
    pub aliases: u32,
    pub preferred: u32,
    pub iana: u32,
    pub region: u32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Aliases {
    pub strings: *const u8,
    pub length: u32,
    pub version: u32,
    pub keys: *const Key,
    pub key_count: u32,
    pub types: *const Type,
    pub type_count: u32,
}

pub struct View<'a> {
    pool: &'a [u8],
    pub keys: &'a [Key],
    pub types: &'a [Type],
}
unsafe fn slice<'a, T>(pointer: *const T, count: u32) -> Result<&'a [T], i32> {
    if count as usize > isize::MAX as usize / std::mem::size_of::<T>()
        || (count != 0 && pointer.is_null())
    {
        return Err(BACKEND);
    }
    if count == 0 {
        return Ok(&[]);
    }
    Ok(unsafe { std::slice::from_raw_parts(pointer, count as usize) })
}
impl Aliases {
    pub unsafe fn view(&self) -> Result<View<'_>, i32> {
        Ok(View {
            pool: unsafe { slice(self.strings, self.length)? },
            keys: unsafe { slice(self.keys, self.key_count)? },
            types: unsafe { slice(self.types, self.type_count)? },
        })
    }
    pub unsafe fn validate(&self) -> Result<(), i32> {
        let view = unsafe { self.view()? };
        if view.text(0)? != ""
            || view.text(self.version)? != icu_locale_data::cldr_tag!()
            || view.keys.len() > u16::MAX as usize
        {
            return Err(BACKEND);
        }
        let mut previous = None;
        for (index, key) in view.keys.iter().enumerate() {
            let name = view.text(key.name)?;
            let value = (key.extension, name);
            let bytes = name.as_bytes();
            if bytes.len() != 2
                || !matches!(key.extension, b'u' | b't')
                || key.value_type > 3
                || key.deprecated > 1
                || key.reserved != 0
                || (key.extension == b'u'
                    && (!bytes[0].is_ascii_alphanumeric() || !bytes[1].is_ascii_lowercase()))
                || (key.extension == b't'
                    && (!bytes[0].is_ascii_lowercase() || !bytes[1].is_ascii_digit()))
                || previous.is_some_and(|old| old >= value)
            {
                return Err(BACKEND);
            }
            view.text(key.aliases)?;
            view.text(key.preferred)?;
            view.canonical_key(index)?;
            previous = Some(value);
        }
        let mut previous = None;
        for (index, row) in view.types.iter().enumerate() {
            let value = (row.key, view.text(row.name)?);
            if row.key as usize >= view.keys.len()
                || row.deprecated > 1
                || row.variable > 1
                || value.1.is_empty()
                || previous.is_some_and(|old| old >= value)
            {
                return Err(BACKEND);
            }
            for field in [row.aliases, row.preferred, row.iana, row.region] {
                view.text(field)?;
            }
            view.terminal_type(index)?;
            previous = Some(value);
        }
        Ok(())
    }
}
impl<'a> View<'a> {
    pub fn text(&self, offset: u32) -> Result<&'a str, i32> {
        let tail = self.pool.get(offset as usize..).ok_or(BACKEND)?;
        let end = tail.iter().position(|byte| *byte == 0).ok_or(BACKEND)?;
        if !tail[..end].is_ascii() {
            return Err(BACKEND);
        }
        std::str::from_utf8(&tail[..end]).map_err(|_| BACKEND)
    }
    pub fn key(&self, extension: u8, name: &str) -> Result<Option<usize>, i32> {
        for (index, key) in self.keys.iter().enumerate() {
            if key.extension == extension && self.text(key.name)? == name {
                return Ok(Some(index));
            }
        }
        Ok(None)
    }
    pub fn canonical_key(&self, mut index: usize) -> Result<usize, i32> {
        for _ in 0..=self.keys.len() {
            let row = &self.keys[index];
            let preferred = self.text(row.preferred)?;
            if preferred.is_empty() {
                return Ok(index);
            }
            index = self.key(row.extension, preferred)?.ok_or(BACKEND)?;
        }
        Err(BACKEND)
    }
    fn terminal_type(&self, mut index: usize) -> Result<usize, i32> {
        for _ in 0..=self.types.len() {
            let row = &self.types[index];
            let preferred = self.text(row.preferred)?;
            if preferred.is_empty() {
                return Ok(index);
            }
            let mut target = None;
            for (i, candidate) in self.types.iter().enumerate() {
                if candidate.key == row.key && self.text(candidate.name)? == preferred {
                    target = Some(i);
                    break;
                }
            }
            index = target.ok_or(BACKEND)?;
        }
        Err(BACKEND)
    }
    pub fn source_type(
        &self,
        key: usize,
        value: &str,
        caseless: bool,
    ) -> Result<Option<usize>, i32> {
        let mut chosen = None;
        let matches = |candidate: &str| {
            if caseless {
                candidate.eq_ignore_ascii_case(value)
            } else {
                candidate == value
            }
        };
        for (index, row) in self.types.iter().enumerate() {
            if row.key as usize != key || row.variable != 0 {
                continue;
            }
            if !matches(self.text(row.name)?)
                && !self
                    .text(row.aliases)?
                    .split_ascii_whitespace()
                    .any(matches)
            {
                continue;
            }
            let terminal = self.terminal_type(index)?;
            if self.types[terminal].variable != 0 {
                continue;
            }
            if chosen.is_some_and(|old| old != terminal) {
                return Err(BACKEND);
            }
            chosen = Some(terminal);
        }
        Ok(chosen)
    }
    pub fn key_name<'b>(&'b self, extension: u8, name: &'b str) -> Result<&'b str, i32> {
        match self.key(extension, name)? {
            Some(index) => self.text(self.keys[self.canonical_key(index)?].name),
            None => Ok(name),
        }
    }
    pub fn normalize<'b>(
        &'b self,
        extension: u8,
        key: &'b str,
        input: &'b str,
    ) -> Result<Cow<'b, str>, i32> {
        let Some(index) = self.key(extension, key)? else {
            return Ok(Cow::Borrowed(input));
        };
        let index = self.canonical_key(index)?;
        let full = self.source_type(index, input, true)?;
        let value = if let Some(row) = full {
            self.text(self.types[row].name)?
        } else {
            input
        };
        if self.keys[index].value_type != 1 || !value.contains('-') {
            return Ok(Cow::Borrowed(value));
        }
        let mut result = String::new();
        for token in value.split('-') {
            let mapped = self.source_type(index, token, true)?;
            let token = if let Some(row) = mapped {
                self.text(self.types[row].name)?
            } else {
                token
            };
            if !result.is_empty() {
                result.push('-');
            }
            result.push_str(token);
        }
        Ok(Cow::Owned(result))
    }
}
