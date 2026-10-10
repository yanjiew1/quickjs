#!/usr/bin/env python3
"""Generate ONE immutable QJTZ binary from the pinned pristine tz submodule.

Only the build's HOST_ZIC is executed; target programs are never used.
The generator performs no network or Git operations.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

from primary import available_named_primaries, backzone_links, cldr_countries, zone_table
from bundle import pack, unpack

PIN = "039ef27cc5f062a2055cb67435d6d71adbefd27d"
RELEASE = "2026e"
DATA = ("africa", "antarctica", "asia", "australasia", "europe",
        "northamerica", "southamerica", "etcetera", "factory", "backward")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tz", type=Path, required=True)
    parser.add_argument("--zic", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--awk", default="awk")
    args = parser.parse_args()
    pin = json.loads((args.tz.parent.parent / "tools/timezone/pin.json").read_text())
    if pin["commit"] != PIN or pin.get("release") != RELEASE or pin.get("tag") != RELEASE:
        raise SystemExit("unexpected tz source pin")
    for name, digest in pin["files"].items():
        if hashlib.sha256((args.tz / name).read_bytes()).hexdigest() != digest:
            raise SystemExit("tz source differs from pin: " + name)
    metadata = Path(__file__).resolve().parent / "cldr"
    cldr_pin = json.loads((metadata / "timezone-pin.json").read_text())
    cldr_xml = (metadata / "timezone.xml").read_bytes()
    if hashlib.sha256(cldr_xml).hexdigest() != cldr_pin["sha256"]:
        raise SystemExit("CLDR geographic metadata differs from pin")
    table_countries, country_rows = zone_table((args.tz / "zone.tab").read_text())
    countries = cldr_countries(cldr_xml, table_countries)
    historical_links = backzone_links((args.tz / "backzone").read_text())
    environment = dict(os.environ, LC_ALL="C", TZ="UTC")
    with tempfile.TemporaryDirectory(prefix="quickjs-tz-") as temporary:
        work = Path(temporary)
        # This is the upstream recommended ECMA-402 tailoring: retain the
        # country zones and their pre-1970 histories from backzone.
        result = subprocess.run([args.awk, "-v", "DATAFORM=main", "-v",
            "PACKRATDATA=backzone", "-v", "PACKRATLIST=zone.tab", "-f",
            "ziguard.awk", *DATA, "backzone"], cwd=args.tz,
            env=environment, check=True, stdout=subprocess.PIPE)
        text = result.stdout.decode("utf-8")
        main_zi = work / "main.zi"
        main_zi.write_bytes(result.stdout)
        output = work / "zoneinfo"
        subprocess.run([str(args.zic.resolve()), "-b", "fat", "-d",
                        str(output), str(main_zi)], env=environment, check=True)
        zones, links = set(), {}
        for line in text.splitlines():
            fields = line.split("#", 1)[0].split()
            if fields and fields[0] == "Zone": zones.add(fields[1])
            if fields and fields[0] == "Link": links[fields[2]] = fields[1]
        primaries = available_named_primaries(zones, links, table_countries,
            country_rows, countries, historical_links)
        names = sorted(primaries)
        compiled = {}
        for name in names:
            primary = primaries[name]
            compiled[name] = (primary, (output / primary).read_bytes())
        # zoneinfo is generation-only scratch and is removed with work.
        # Names, primary links and deduplicated payloads have one final file.
        binary = pack(compiled)
        records = unpack(binary)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(binary)
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(json.dumps({"pin": PIN, "release": RELEASE,
            "format": "QJTZ", "format_version": 1, "payload_format": "fat TZif",
            "primary_algorithm": "ECMA-402 AvailableNamedTimeZoneIdentifiers",
            "country_metadata": {"cldr_commit": cldr_pin["commit"],
                "xml_sha256": cldr_pin["sha256"]},
            "packratdata": "backzone", "packratlist": "zone.tab",
            "leaps": False, "range_limited": False,
            "data_sha256": hashlib.sha256(binary).hexdigest(),
            "binary_size": len(binary), "final_binary_files": 1,
            "runtime_tzcode": False,
            "records": records}, indent=2) + "\n")

if __name__ == "__main__":
    main()
