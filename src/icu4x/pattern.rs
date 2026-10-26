/* Private ICU4X decoded single-placeholder patterns.
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

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Text {
    pub(crate) data: *const u8,
    pub(crate) length: usize,
}
impl From<&str> for Text {
    fn from(value: &str) -> Self {
        Self {
            data: value.as_ptr(),
            length: value.len(),
        }
    }
}

// Keep decoded literal spans distinct from the numeric placeholder. This is
// shared by compact, relative, unit and date metadata; no caller reparses braces.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SinglePattern {
    pub(crate) prefix: Text,
    pub(crate) suffix: Text,
    pub(crate) has_number: u32,
}
pub(crate) fn single_pattern(
    pattern: &icu_pattern::SinglePlaceholderPattern,
) -> Result<SinglePattern, i32> {
    use icu_pattern::PatternItem;
    let mut result = SinglePattern {
        prefix: "".into(),
        suffix: "".into(),
        has_number: 0,
    };
    for item in pattern.iter() {
        match item {
            PatternItem::Literal(value) => {
                let target = if result.has_number == 0 {
                    &mut result.prefix
                } else {
                    &mut result.suffix
                };
                if target.length != 0 {
                    return Err(BACKEND);
                }
                *target = value.into();
            }
            PatternItem::Placeholder(_) => {
                if result.has_number != 0 {
                    return Err(BACKEND);
                }
                result.has_number = 1;
            }
        }
    }
    Ok(result)
}
