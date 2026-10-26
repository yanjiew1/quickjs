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
use std::borrow::Cow;

// ICU4X Language stores only two or three letters; ECMA's Unicode grammar
// also accepts unregistered five-to-eight-letter primary languages. Mask only
// that field with a reserved private-use language, validate the entire remaining
// identifier using the public parser, then restore the canonical primary.
pub(crate) struct LanguageInput<'a> {
    text: Cow<'a, str>,
    primary: Option<&'a str>,
}

impl<'a> LanguageInput<'a> {
    pub(crate) fn new(input: &'a str) -> Self {
        let primary = input.split('-').next().unwrap_or("");
        if (5..=8).contains(&primary.len()) && primary.bytes().all(|c| c.is_ascii_alphabetic()) {
            Self {
                text: Cow::Owned(format!("qaa{}", &input[primary.len()..])),
                primary: Some(primary),
            }
        } else {
            Self {
                text: Cow::Borrowed(input),
                primary: None,
            }
        }
    }
    pub(crate) fn as_str(&self) -> &str {
        &self.text
    }
    pub(crate) fn masked(&self) -> bool {
        self.primary.is_some()
    }
    pub(crate) fn restore(&self, mut output: String) -> String {
        if let Some(primary) = self.primary {
            let end = output.find('-').unwrap_or(output.len());
            output.replace_range(..end, &primary.to_ascii_lowercase());
        }
        output
    }
}
