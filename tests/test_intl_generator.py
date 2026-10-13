#!/usr/bin/env python3
"""Check pinned collector loading and bounded binary layout without CLDR IO."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "tools/intl_gen/combined_bundle.py"
# Match normal script invocation so authenticated sibling imports resolve.
sys.path.insert(0, str(PATH.parent))
SPEC = importlib.util.spec_from_file_location("qjs_intl_combined_generator", PATH)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)

class GeneratorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.modules, cls.pins = GENERATOR.load_modules()

    def test_verified_collectors(self):
        self.assertEqual(set(self.modules), set(GENERATOR.MODULE_ORDER))
        self.assertEqual(set(self.pins), {name + ".py" for name in self.modules})

    def test_wire_layout_and_padding(self):
        blob, inventory = GENERATOR.encode_blob(
            {1: b"abc", 2: bytes(range(32))}, {1: 1, 2: 32}, {1, 2},
            self.modules["metadata"], "17.0.0")
        self.assertEqual(blob[:8], b"QJSINTL\0")
        self.assertEqual(struct.unpack_from("<HH", blob, 8), (1, 3))
        self.assertEqual(struct.unpack_from("<I", blob, 16)[0], len(blob))
        self.assertEqual(inventory[0]["offset"], 112)
        self.assertEqual(inventory[1]["offset"], 116)
        self.assertEqual(blob[112:116], b"abc\0")
        self.assertEqual(blob[116:], bytes(range(32)))

    def test_reject_missing_and_misaligned_records(self):
        for sections, widths, required in [
            ({1: b"a"}, {1: 1}, {1, 2}),
            ({1: b"a"}, {1: 2}, {1}),
            ({1: b"a", 3: b"b"}, {1: 1}, {1})
        ]:
            with self.assertRaises(ValueError):
                GENERATOR.encode_blob(sections, widths, required,
                                      self.modules["metadata"], "17.0.0")

if __name__ == "__main__":
    unittest.main()
