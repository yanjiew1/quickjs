#!/bin/sh
# Shared CI evidence/reporting; workflow steps retain execution policy.
set -e

report_status=0
for prefix do
  if test -f "${prefix}_timings.jsonl"; then
    python3 tools/ci/report_test262_timings.py "${prefix}_timings.jsonl" \
      --csv "${prefix}_timings.csv" --markdown "${prefix}_timings.md" || report_status=$?
    if test -f "${prefix}_timings.md"; then
      cat "${prefix}_timings.md" >> "$GITHUB_STEP_SUMMARY"
    fi
  fi
done
exit "$report_status"
