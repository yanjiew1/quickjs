"""Complete formatter fallback identities without changing source inheritance."""
import re

from parents import ParentGraph


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
