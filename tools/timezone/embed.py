#!/usr/bin/env python3
"""Derive C embedding/index views from the one final timezone-data.bin."""
import argparse
from pathlib import Path
from bundle import unpack


def render(binary):
    records = unpack(binary)
    lines = ["/* Derived from the complete QJTZ binary; do not edit. */",
             "const unsigned char qjs_tz_embedded_data[] = {"]
    for start in range(0, len(binary), 16):
        lines.append("    " + ",".join("0x%02x" % byte for byte in binary[start:start + 16]) + ",")
    lines += ["};", "const size_t qjs_tz_embedded_size = sizeof(qjs_tz_embedded_data);",
              "const QJSTzRecord qjs_tz_records[] = {"]
    for record in records:
        lines.append("    {(const char *)qjs_tz_embedded_data + %d, "
                     "(const char *)qjs_tz_embedded_data + %d, %d, %d}," %
                     (record["identifier_offset"], record["primary_offset"],
                      record["offset"], record["length"]))
    lines += ["};", "const size_t qjs_tz_record_count = sizeof(qjs_tz_records) / sizeof(*qjs_tz_records);", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    text = render(args.input.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="ascii")


if __name__ == "__main__":
    main()
