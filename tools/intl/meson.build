# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# Prepared-pack codecs, embedding and authored fixtures share this conservative
# import closure. Acquisition and final source-preparation commands are not
# dependencies of these targets; fixtures add their own source helpers below.
intl_tool_dependencies = files(
  '../data/_support/io.py',
  'acquire.py',
  'availability.py',
  'bundle.py',
  'calendar_era_tables.py',
  'calendar_eras.py',
  'calendar_name_tables.py',
  'calendar_names.py',
  'calendar_sources.py',
  'cldr_xml.py',
  'collation_archive.py',
  'collation_bindings.py',
  'collation_bundle.py',
  'collation_contexts.py',
  'collation_groups.py',
  'collation_programs.py',
  'collation_root.py',
  'collation_rules.py',
  'collation_tables.py',
  'compact_pattern_syntax.py',
  'compact_patterns.py',
  'compact_tables.py',
  'currencies.py',
  'currency_tables.py',
  'date_available_formats.py',
  'date_available_tables.py',
  'date_combination_tables.py',
  'date_combinations.py',
  'date_field_names.py',
  'date_field_tables.py',
  'date_interval_tables.py',
  'date_intervals.py',
  'date_pattern_syntax.py',
  'date_patterns.py',
  'date_style_tables.py',
  'dayperiod_codes.py',
  'dayperiod_rules.py',
  'dayperiod_tables.py',
  'display_names.py',
  'display_tables.py',
  'duration_patterns.py',
  'duration_tables.py',
  'embed.py',
  'list_patterns.py',
  'literal_template.py',
  'locale_metadata.py',
  'locale_tables.py',
  'number_layout.py',
  'number_layout_tables.py',
  'number_pattern_syntax.py',
  'number_pattern_tables.py',
  'number_patterns.py',
  'number_symbol_tables.py',
  'number_symbols.py',
  'numbering.py',
  'numbering_tables.py',
  'parents.py',
  'plural_rules.py',
  'plural_tables.py',
  'preference_tables.py',
  'preferences.py',
  'relative_patterns.py',
  'relative_tables.py',
  'resolve.py',
  'scalar_trie.py',
  'scalar_trie_tables.py',
  'segment_alphabet.py',
  'segment_dfa.py',
  'segment_dictionary.py',
  'segment_dictionary_sources.py',
  'segment_dictionary_tables.py',
  'segment_dictionary_trie.py',
  'segment_expression.py',
  'segment_lower.py',
  'segment_nfa.py',
  'segment_profile.py',
  'segment_program.py',
  'segment_program_tables.py',
  'segment_rules.py',
  'segment_sets.py',
  'segment_sources.py',
  'segment_suppression_tables.py',
  'tables.py',
  'timezone_name_tables.py',
  'timezone_names.py',
  'timezone_presentation_tables.py',
  'timezone_syntax.py',
  'unit_grammar.py',
  'unit_grammar_tables.py',
  'unit_patterns.py',
  'unit_tables.py',
  'zone_pattern_syntax.py',
)
