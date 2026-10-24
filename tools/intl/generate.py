#!/usr/bin/env python3
"""Prepare deterministic minimal/native Intl sections from supplied CLDR."""
import argparse
import copy
import json
from pathlib import Path

import acquire
import availability
import bundle
import calendar_names
import calendar_name_tables
import calendar_eras
import calendar_era_tables
import collation_contexts
import collation_bundle
import collation_root
import collation_tables
import compact_patterns
import compact_tables
import currencies
import currency_tables
import dayperiod_rules
import dayperiod_tables
import date_intervals
import date_interval_tables
import timezone_names
import timezone_name_tables
import timezone_presentation
import timezone_presentation_tables
import date_available_formats
import date_available_tables
import date_field_names
import date_field_tables
import date_combinations
import date_combination_tables
import date_patterns
import date_style_tables
import display_names
import display_tables
import list_patterns
import locale_metadata
import locale_tables
import numbering
import numbering_tables
import number_symbols
import number_symbol_tables
import number_patterns
import number_pattern_tables
import number_layout
import number_layout_tables
import parents
import preferences
import preference_tables
import plural_rules
import plural_tables
import relative_patterns
import relative_tables
import segment_bundle
import duration_patterns
import duration_tables
import tables
import unit_patterns
import unit_tables
import unit_grammar
import unit_grammar_tables
from source_inputs import Inputs


def generate(sources, profile, compiled_collation=None, compiled_segmentation=None,
             segmentation_manifest=None):
    if profile not in bundle.PROFILES:
        raise ValueError('unknown native Intl data profile')
    if compiled_segmentation is not None and profile != 'native':
        raise ValueError('compiled segmentation requires the native profile')
    if (compiled_segmentation is None) != (segmentation_manifest is None):
        raise ValueError('compiled segmentation and its manifest are required together')
    inputs_read = Inputs(sources)
    # Authored fixtures can omit optional service sections. The production CLI
    # requires the already compiled profile; generation never invents a fallback.
    compiled_section = None
    if compiled_collation is not None:
        compiled_section = collation_bundle.pair(
            inputs_read, profile, bytes.fromhex(sources.pin['manifest_sha256']), compiled_collation)

    inputs_read.read('LICENSE')
    graph, fallback_aliases = availability.complete(parents.load(inputs_read))
    tags = availability.profile_locales(graph, profile)
    segmentation_sections, segmentation_inputs = {}, None
    if compiled_segmentation is not None:
        segmentation_sections, segmentation_inputs = segment_bundle.pair(
            inputs_read, sources.manifest['files'], graph, tags, compiled_segmentation,
            segmentation_manifest)
    locale_data = locale_metadata.collect(inputs_read, graph)
    preference_data = preferences.collect(inputs_read)
    numbering_data = numbering.collect(inputs_read, graph)
    symbol_data = number_symbols.collect(inputs_read, graph)
    pattern_data = number_patterns.collect(inputs_read, graph)
    compact_data = compact_patterns.collect(inputs_read, graph)
    currency_data = currencies.collect(inputs_read, graph)
    layout_data = number_layout.collect(inputs_read, graph, numbering_data)
    unit_data = unit_patterns.collect(inputs_read, graph)
    grammar_data = unit_grammar.collect(inputs_read, graph)
    plural_data = plural_rules.collect(inputs_read, graph)
    relative_data = relative_patterns.collect(inputs_read, graph)
    duration_data = duration_patterns.collect(inputs_read, graph)
    date_field_data = date_field_names.collect(inputs_read, graph)
    date_style_data = date_patterns.collect(inputs_read, graph)
    date_combination_data = date_combinations.collect(inputs_read, graph)
    calendar_name_data = calendar_names.collect(inputs_read, graph)
    date_available_data = date_available_formats.collect(inputs_read, graph)
    date_interval_data = date_intervals.collect(inputs_read, graph)
    timezone_name_data = timezone_names.collect(inputs_read, graph)
    timezone_presentation_data = timezone_presentation_tables.runtime_records(
        timezone_presentation.collect(inputs_read, locale_data))
    dayperiod_data = dayperiod_rules.collect(inputs_read)
    calendar_era_data = calendar_eras.collect(inputs_read)
    collation_data = collation_root.collect(inputs_read)
    collation_context_data = collation_contexts.collect(inputs_read, collation_data)
    display_data = display_names.collect(inputs_read, graph)
    lists = list_patterns.collect(inputs_read, graph)
    # Only formatter locale rows are reduced for minimal. Global identity,
    # preferences and timezone metadata retain their full source coverage.
    selected_tags = set(tags)
    symbol_data['rows'] = tuple(row for row in symbol_data['rows'] if row[0] in selected_tags)
    pattern_data['rows'] = tuple(row for row in pattern_data['rows'] if row[0] in selected_tags)
    pattern_data['grouping'] = tuple(row for row in pattern_data['grouping'] if row[0] in selected_tags)
    compact_data = compact_tables.select(compact_data, tags)
    compact_prepared = compact_tables.prepare(compact_data, tags, numbering_data)
    currency_data = currency_tables.select(currency_data, tags)
    pattern_data['overrides'] = currency_tables.patterns(currency_data)
    layout_data['rows'] = tuple(row for row in layout_data['rows'] if row[0] in selected_tags)
    layout_data['root_fields'] = tuple(row for row in layout_data['root_fields'] if row[0] in selected_tags)
    layout_data['currency_units'] = tuple(row for row in layout_data['currency_units'] if row[0] in selected_tags)
    unit_data = unit_tables.select(unit_data, tags)
    grammar_data = unit_grammar_tables.select(grammar_data, tags)
    relative_data = relative_tables.select(relative_data, tags)
    duration_data = duration_tables.select(duration_data, tags)
    date_field_data = date_field_tables.select(date_field_data, tags)
    date_style_data = date_style_tables.select(date_style_data, tags)
    date_combination_data = date_combination_tables.select(date_combination_data, tags)
    calendar_name_data = calendar_name_tables.select(calendar_name_data, tags)
    date_available_data = date_available_tables.select(date_available_data, tags)
    date_interval_data = date_interval_tables.select(date_interval_data, tags)
    timezone_name_data = timezone_name_tables.select(timezone_name_data, tags)
    display_data = display_tables.select(display_data, tags)
    plural_data['rows'] = tuple(row for row in plural_data['rows'] if row[0] in selected_tags)
    selected = [row for row in lists['rows'] if row[0] in selected_tags]
    contexts = [row for row in lists['contexts'] if row[0] in selected_tags]
    pool_strings = {''}
    pool_strings.update(tags)
    pool_strings.update(locale_tables.strings(locale_data))
    pool_strings.update(preference_tables.strings(preference_data))
    pool_strings.update(numbering_tables.strings(numbering_data))
    pool_strings.update(number_symbol_tables.strings(symbol_data))
    pool_strings.update(number_pattern_tables.strings(pattern_data))
    pool_strings.update(compact_tables.strings(compact_prepared))
    pool_strings.update(currency_tables.strings(currency_data))
    pool_strings.update(number_layout_tables.strings(layout_data))
    pool_strings.update(unit_tables.strings(unit_data))
    pool_strings.update(unit_grammar_tables.strings(grammar_data))
    pool_strings.update(plural_tables.strings(plural_data))
    pool_strings.update(relative_tables.strings(relative_data))
    pool_strings.update(duration_tables.strings(duration_data))
    pool_strings.update(date_field_tables.strings(date_field_data))
    pool_strings.update(date_style_tables.strings(date_style_data))
    pool_strings.update(date_combination_tables.strings(date_combination_data))
    pool_strings.update(calendar_name_tables.strings(calendar_name_data))
    pool_strings.update(date_available_tables.strings(date_available_data))
    pool_strings.update(date_interval_tables.strings(date_interval_data))
    pool_strings.update(timezone_name_tables.strings(timezone_name_data))
    pool_strings.update(timezone_presentation_tables.strings(timezone_presentation_data))
    pool_strings.update(dayperiod_tables.strings(dayperiod_data))
    pool_strings.update(calendar_era_tables.strings(calendar_era_data))
    pool_strings.update(display_tables.strings(display_data))
    pool_strings.update(text for locale, variant, values in selected for text in values)
    pool_strings.update(text for locale, variant, kind, pair, end in contexts
                        for text in (pair, end))
    strings = tuple(sorted(pool_strings, key=lambda text: text.encode('utf-8')))
    string_ids = {text: index for index, text in enumerate(strings)}
    sections = {bundle.STRINGS: tables.encode_strings(strings),
                bundle.LOCALES: tables.encode_locales(tags, graph, string_ids),
                bundle.LISTS: tables.encode_lists(selected, tags, string_ids),
                bundle.LIST_CONTEXTS: tables.encode_contexts(contexts, tags, string_ids)}
    sections.update(locale_tables.encode(locale_data, string_ids))
    sections.update(preference_tables.encode(preference_data, string_ids))
    sections.update(numbering_tables.encode(numbering_data, string_ids))
    sections[bundle.NUMBER_SYMBOLS] = number_symbol_tables.encode(symbol_data, tags,
                                                               numbering_data, string_ids)
    sections[bundle.PLURALS] = plural_tables.encode(plural_data, tags, string_ids)
    sections[bundle.NUMBER_PATTERNS] = number_pattern_tables.encode(pattern_data, tags,
                                                                 numbering_data, string_ids)
    sections[bundle.COMPACT_PATTERNS] = compact_tables.encode(compact_prepared, string_ids)
    sections[bundle.CURRENCIES] = currency_tables.encode(currency_data, tags, string_ids)
    sections[bundle.NUMBER_LAYOUTS] = number_layout_tables.encode(layout_data, tags,
                                                               numbering_data, string_ids)
    sections[bundle.UNIT_PATTERNS] = unit_tables.encode(unit_data, tags, string_ids)
    sections[bundle.UNIT_GRAMMAR] = unit_grammar_tables.encode(grammar_data, tags, string_ids)
    sections[bundle.RELATIVE_PATTERNS] = relative_tables.encode(relative_data, tags, string_ids)
    sections[bundle.DURATION_PATTERNS] = duration_tables.encode(duration_data, tags, string_ids)
    sections[bundle.DATE_FIELD_NAMES] = date_field_tables.encode(date_field_data, tags, string_ids)
    sections[bundle.DATE_STYLES] = date_style_tables.encode(date_style_data, tags, string_ids)
    sections[bundle.DATE_COMBINATIONS] = date_combination_tables.encode(date_combination_data, tags, string_ids)
    sections[bundle.CALENDAR_NAMES] = calendar_name_tables.encode(calendar_name_data, tags, string_ids)
    sections[bundle.DATE_AVAILABLE_FORMATS] = date_available_tables.encode(date_available_data, tags, string_ids)
    sections[bundle.DATE_INTERVALS] = date_interval_tables.encode(date_interval_data, tags, string_ids)
    sections[bundle.TIMEZONE_NAMES] = timezone_name_tables.encode(timezone_name_data, tags, string_ids)
    sections[bundle.TIMEZONE_PRESENTATION] = timezone_presentation_tables.encode(timezone_presentation_data, string_ids)
    sections[bundle.DAYPERIOD_RULES] = dayperiod_tables.encode(dayperiod_data, string_ids)
    sections[bundle.CALENDAR_ERAS] = calendar_era_tables.encode(calendar_era_data, string_ids)
    # Root UCA mappings are global scalar data, not locale-row projection.
    sections[bundle.COLLATION_ROOT] = collation_tables.encode(collation_data)
    sections[bundle.COLLATION_CONTEXTS] = collation_contexts.encode(collation_context_data, collation_data)
    if compiled_section is not None:
        sections[bundle.COMPILED_COLLATION] = compiled_section
    sections[bundle.DISPLAY_NAMES] = display_tables.encode(display_data, tags, string_ids)
    sections.update(segmentation_sections)
    inputs = {'schema': list(bundle.VERSION), 'profile': profile,
              'cldr': copy.deepcopy(sources.pin),
              'sources': {name: sources.manifest['files'][name] for name in sorted(inputs_read.consumed)},
              'policy': lists['policy'] + '; formatter fallback registry policy 1',
              'formatter_fallbacks': tuple(row for row in fallback_aliases if row[0] in selected_tags),
              'sections': {str(identity): {'count': count, 'sha256': acquire.digest(data)}
                           for identity, (count, data) in sorted(sections.items())}}
    if segmentation_inputs is not None:
        inputs['segmentation'] = segmentation_inputs
    generation = bytes.fromhex(acquire.digest(acquire.json_bytes(inputs)))
    binary = bundle.pack(sections, generation, profile)
    tables.validate(binary)
    manifest = {'format': 1, 'inputs': inputs, 'generation': generation.hex(),
                'sha256': acquire.digest(binary), 'bytes': len(binary),
                'locales': len(tags), 'strings': len(strings),
                'complete_services': [],
                'note': ('Prepared locale metadata, preferences, list templates, numeric symbols/patterns, '
                         'compact maps, currency/layout/unit facts, division grammar, relative/duration '
                         'source facts, date-field labels, date/time styles and connectors, calendar names, '
                         'available date patterns, append templates, interval and localized timezone source layers, '
                         'supplemental metazone presentation, day-period clock facts and explicit era name keys, '
                         'root UCA mappings, optional bound compiled collation, '
                         'layered display names and plural rules; '
                         'not formatter capability '
                         'or full Intl coverage.')}
    return binary, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--source-pin', type=Path,
                        help='explicit alternate source pin for release/Git comparison')
    parser.add_argument('--profile', choices=tuple(bundle.PROFILES), required=True)
    parser.add_argument('--compiled-collation', type=Path, required=True)
    parser.add_argument('--compiled-segmentation', type=Path)
    parser.add_argument('--segmentation-manifest', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    if args.profile == 'native' and args.compiled_segmentation is None:
        parser.error('--compiled-segmentation is required for the native profile')
    if args.profile != 'native' and args.compiled_segmentation is not None:
        parser.error('--compiled-segmentation applies only to the native profile')
    if (args.compiled_segmentation is None) != (args.segmentation_manifest is None):
        parser.error('--compiled-segmentation and --segmentation-manifest are required together')
    binary, manifest = generate(acquire.Sources(args.sources, acquire.load_pin(args.source_pin)), args.profile,
                                args.compiled_collation.read_bytes(),
                                args.compiled_segmentation.read_bytes()
                                if args.compiled_segmentation is not None else None,
                                json.loads(args.segmentation_manifest.read_text())
                                if args.segmentation_manifest is not None else None)
    acquire.atomic_write(args.output, binary)
    acquire.atomic_write(args.manifest, acquire.json_bytes(manifest))


if __name__ == '__main__':
    main()
