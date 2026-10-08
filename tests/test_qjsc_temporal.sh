#!/bin/sh
# Native qjsc link metadata and Temporal intrinsic registration regression.
set -eu
qjsc_path=$1
scratch_dir=.obj/test-qjsc-temporal-$$
mkdir -p "$scratch_dir"
trap 'rm -rf "$scratch_dir"' EXIT HUP INT TERM
"$qjsc_path" -o "$scratch_dir/check" tests/test_qjsc_temporal.js
"$scratch_dir/check"
