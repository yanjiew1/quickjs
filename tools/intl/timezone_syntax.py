"""Shared CLDR presentation ID syntax, without runtime canonicalization."""
import re

ZONE = re.compile(r'[A-Za-z0-9_+.-]+(?:/[A-Za-z0-9_+.-]+)*')
METAZONE = re.compile(r'[A-Za-z][A-Za-z0-9_]*')
