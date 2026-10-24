#!/bin/sh
# Shared CI evidence/reporting; workflow steps retain execution policy.
set -e

for stage in repo-unit repo-integration; do
  log="build/meson-logs/$stage.txt"
  if test -f "$log"; then
    printf '### %s\n\n```text\n' "$stage" >> "$GITHUB_STEP_SUMMARY"
    tail -n 20 "$log" >> "$GITHUB_STEP_SUMMARY"
    printf '\n```\n' >> "$GITHUB_STEP_SUMMARY"
  fi
done
