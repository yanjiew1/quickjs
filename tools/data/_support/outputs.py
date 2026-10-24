"""Deterministic maintenance receipts and shared atomic output writing."""
import json

from .io import write_atomic


def encoded(value):
    return (json.dumps(value, sort_keys=True, indent=2) + '\n').encode('utf-8')
