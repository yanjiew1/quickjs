#!/usr/bin/env python3
"""Derive a config matching explicit native feature switches, without new failure masks."""
import argparse
from pathlib import Path


def derive(source, intl, temporal, preserve_reportfile=False):
    output = ["# Derived for explicit CONFIG_INTL={} CONFIG_TEMPORAL={}.\n".format(intl, temporal)]
    for line in source.splitlines(keepends=True):
        stripped = line.strip()
        name = stripped.split("=", 1)[0]
        if name == "Intl.Era-monthcode":
            output.append(name + ("\n" if intl == "y" or temporal == "y" else "=skip\n"))
        elif name.startswith("Intl") or name in ("intl-normative-optional", "canonical-tz"):
            output.append(name + ("\n" if intl == "y" else "=skip\n"))
        elif name == "Temporal":
            output.append(name + ("\n" if temporal == "y" else "=skip\n"))
        elif stripped == "test262/test/intl402/" and intl == "y":
            continue
        elif stripped == "# intl not supported" and intl == "y":
            continue
        elif name == "reportfile" and not preserve_reportfile:
            report = "test262_intl_report.txt" if intl == "y" else (
                "test262_temporal_report.txt" if temporal == "y" else "test262_report.txt")
            output.append("reportfile=" + report + "\n")
        else:
            output.append(line)
    return "".join(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--intl", choices=("y", "n", ""), required=True)
    parser.add_argument("--temporal", choices=("y", "n", ""), required=True)
    parser.add_argument("--source", default="test262.conf", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--preserve-reportfile", action="store_true",
                        help="retain the source report path for ordinary Make targets")
    args = parser.parse_args()
    args.output.write_text(derive(args.source.read_text(), args.intl, args.temporal,
                                  args.preserve_reportfile))
