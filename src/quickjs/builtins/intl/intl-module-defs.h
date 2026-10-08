/*
 * QuickJS native internationalization support
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
/* Registered native services. Keep this manifest and INTL_SRCS aligned.
   During incremental preparation include only actual implemented services.
   The complete CONFIG_INTL build contains every entry below. */
JS_INTL_MODULE(-1, JS_CLASS_INTL_LOCALE, js_intl_init_locale)
JS_INTL_MODULE(JS_INTL_COLLATOR, JS_CLASS_INTL_COLLATOR, js_intl_init_collator)
JS_INTL_MODULE(JS_INTL_SEGMENTER, JS_CLASS_INTL_SEGMENTER, js_intl_init_segmenter)
JS_INTL_MODULE(JS_INTL_DATE_TIME_FORMAT, JS_CLASS_INTL_DATE_TIME_FORMAT, js_intl_init_date_time_format)
JS_INTL_MODULE(JS_INTL_LIST_FORMAT, JS_CLASS_INTL_LIST_FORMAT, js_intl_init_list_format)
JS_INTL_MODULE(JS_INTL_DISPLAY_NAMES, JS_CLASS_INTL_DISPLAY_NAMES, js_intl_init_display_names)
JS_INTL_MODULE(JS_INTL_NUMBER_FORMAT, JS_CLASS_INTL_NUMBER_FORMAT, js_intl_init_number_format)
JS_INTL_MODULE(JS_INTL_PLURAL_RULES, JS_CLASS_INTL_PLURAL_RULES, js_intl_init_plural_rules)
JS_INTL_MODULE(JS_INTL_RELATIVE_TIME_FORMAT, JS_CLASS_INTL_RELATIVE_TIME_FORMAT, js_intl_init_relative_time_format)
JS_INTL_MODULE(JS_INTL_DURATION_FORMAT, JS_CLASS_INTL_DURATION_FORMAT, js_intl_init_duration_format)
