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

"""Complete formatter fallback identities without changing source inheritance."""
import re

from parents import ParentGraph


def profile_locales(graph, profile):
    """Project formatter/service identities; never change source parent graphs."""
    if profile == 'native':
        return graph.tags
    if profile != 'minimal':
        raise ValueError('unknown native Intl data profile')

    # Keep the explicit US identity as well as generic English. Sparse authored
    # fixtures need not contain every production seed.
    seeds = (locale for locale in ('en', 'en-US') if locale in graph.tags)
    return tuple(sorted({tag for locale in seeds for tag in graph.chain(locale)}))


def complete(graph):
    """Add required language-region entries, mapping them to full CLDR records.

    A source language-script-region entry requires a scriptless entry under
    ECMA-402 Available Locales List. When several scripts share that entry,
    prefer its likely script, then a stable source identity. Existing source
    entries and their inheritance always take precedence. The returned graph
    is a formatter registry, not a claim about CLDR's original parent records.
    """
    candidates = {}
    tags = set(graph.tags)
    for tag in graph.tags:
        parts = tag.split('-')
        if (len(parts) == 3 and re.fullmatch('[A-Z][a-z]{3}', parts[1]) and
                re.fullmatch(r'(?:[A-Z]{2}|[0-9]{3})', parts[2])):
            target = parts[0] + '-' + parts[2]
            if target not in tags:
                candidates.setdefault(target, []).append(tag)
    aliases = {}
    for target, records in sorted(candidates.items()):
        language = target.split('-')[0]
        likely = graph.likely.get(target, graph.likely.get(language, graph.likely['und']))
        script = likely.split('-')[1]
        aliases[target] = min(records, key=lambda tag: (tag.split('-')[1] != script, tag))
    general = dict(graph.general)
    general.update(aliases)
    result = ParentGraph(graph.source_locales, general, graph.components, graph.likely,
                         graph.default_content, graph.nonlikely_script)
    return result, tuple(sorted(aliases.items()))
