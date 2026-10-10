#!/usr/bin/env python3
"""Data-only generator checks; requires no compiler, engine, ICU, or network."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("windows_zones_generator", ROOT / "tools/gen-windows-zones.py")
GEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GEN)
CLDR = ROOT / "tools/timezone/cldr"
TZ_SOURCE = None
TZ_PIN = None


def xml(rows):
    return ("<supplementalData><windowsZones><mapTimezones>" + rows +
            "</mapTimezones></windowsZones></supplementalData>").encode()


def entry(key="Example Time", country="001", targets="Etc/UTC"):
    return '<mapZone other="%s" territory="%s" type="%s"/>' % (key, country, targets)


class GeneratorTests(unittest.TestCase):
    def test_order_and_determinism(self):
        rows = [entry(), entry(country="CA", targets="America/Vancouver America/Winnipeg")]
        available = {"Etc/UTC", "America/Vancouver", "America/Winnipeg"}
        first = GEN.parse_mapping(xml("".join(rows)), available)
        second = GEN.parse_mapping(xml("".join(reversed(rows))), available)
        self.assertEqual(first["example time"]["CA"], ("America/Vancouver", "America/Winnipeg"))
        self.assertEqual(GEN.encode(first), GEN.encode(second))
        blob = GEN.encode(first)
        self.assertEqual(blob[:4], b"WTZ1")
        self.assertEqual(struct.unpack_from("<I", blob, 4)[0], len(blob))
        self.assertIn(b"America/Vancouver America/Winnipeg\0", blob)

    def test_reject_duplicate_pair(self):
        with self.assertRaisesRegex(ValueError, "duplicate Windows key/territory"):
            GEN.parse_mapping(xml(entry() + entry()), {"Etc/UTC"})

    def test_reject_case_collision(self):
        with self.assertRaisesRegex(ValueError, "collide"):
            GEN.parse_mapping(xml(entry() + entry(key="EXAMPLE TIME", country="US")), {"Etc/UTC"})

    def test_reject_missing_global(self):
        with self.assertRaisesRegex(ValueError, "001"):
            GEN.parse_mapping(xml(entry(country="CA")), {"Etc/UTC"})

    def test_reject_bad_identifiers(self):
        for target in ("", "../UTC", "America//Vancouver", "America/../UTC", "Europe/Unknown", "Etc/UTC Etc/UTC"):
            with self.subTest(target=target), self.assertRaises(ValueError):
                GEN.parse_mapping(xml(entry(targets=target)), {"Etc/UTC"})

    def test_reject_malformed_rows(self):
        for row in (entry(country="ca"), entry(country="123"), entry(key=""),
                    entry(key="Example&#x80;Time"), '<mapZone other="X" territory="001"/>',
                    '<notMapZone other="X" territory="001" type="Etc/UTC"/>'):
            with self.subTest(row=row), self.assertRaises(ValueError):
                GEN.parse_mapping(xml(row), {"Etc/UTC"})

    def test_pool_limit(self):
        groups = {"example time": {"001": ("A" * 65535,)}}
        with self.assertRaisesRegex(ValueError, "pool"):
            GEN.encode(groups)

    def test_pinned_sources_and_generated_bytes(self):
        pin = json.loads((CLDR / "pin.json").read_text())
        data = (CLDR / "windowsZones.xml").read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(), pin["xml_sha256"])
        available = GEN.tz_identifiers(TZ_SOURCE, TZ_PIN)
        groups = GEN.parse_mapping(data, available)
        self.assertEqual(groups["pacific standard time"]["001"], ("America/Los_Angeles",))
        self.assertEqual(groups["pacific standard time"]["CA"], ("America/Vancouver",))
        self.assertEqual(groups["central standard time"]["CA"][0], "America/Winnipeg")
        self.assertEqual(groups["central standard time"]["MX"][0], "America/Matamoros")
        self.assertGreater(len(groups["alaskan standard time"]["US"]), 1)
        for territories in groups.values():
            for targets in territories.values():
                self.assertTrue(set(targets) <= available)
        expected = GEN.generate(data, pin, TZ_SOURCE, TZ_PIN)
        self.assertEqual(expected, (ROOT / "src/timezone/windows-zone-data.inc").read_text())
        broken = dict(pin, xml_sha256="0" * 64)
        with self.assertRaisesRegex(ValueError, "CLDR XML hash mismatch"):
            GEN.generate(data, broken, TZ_SOURCE, TZ_PIN)
        broken = dict(TZ_PIN, files=dict(TZ_PIN["files"], africa="0" * 64))
        with self.assertRaisesRegex(ValueError, "tz source hash mismatch"):
            GEN.tz_identifiers(TZ_SOURCE, broken)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tz-source", type=Path, required=True)
    parser.add_argument("--tz-pin", type=Path, required=True)
    args = parser.parse_args()
    TZ_SOURCE = args.tz_source
    TZ_PIN = json.loads(args.tz_pin.read_text())
    unittest.main(argv=[__file__], verbosity=2)
