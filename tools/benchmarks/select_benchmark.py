#!/usr/bin/env python3
"""Select hosted benchmark cadence and derive the exact first-parent baseline."""

import argparse
from pathlib import Path
import re
import subprocess


IMPACT_PREFIXES = ("src/", "include/", "tools/benchmarks/", "tools/build/", "tools/timezone/", "tools/intl/", "build-aux/")
IMPACT_FILES = {"Makefile", "meson.build", "meson_options.txt", "tests/microbench.js", ".github/workflows/benchmarks.yml"}


def selection(event, commit_count, paths, before_count=None, branch=""):
    if event == "push" and branch in ("main", "main-new", "temporal", "intl"):
        return True, "Latest formal branch tip"
    if event == "workflow_dispatch":
        return True, "Explicit dispatch"
    if commit_count < 1:
        raise ValueError("Positive first-parent commit count required")
    if before_count is None or not 0 < before_count < commit_count:
        return True, "Uncertain or rewritten push history; conservatively selected"
    if commit_count // 25 > before_count // 25:
        return True, "Routine every 25 first-parent commits"
    if any(path in IMPACT_FILES or path.startswith(IMPACT_PREFIXES) for path in paths):
        return True, "Likely performance impact in runtime, build, or workload sources"
    return False, "Between routine cadence; no likely performance impact path"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--candidate-revision", required=True)
    parser.add_argument("--before-revision", default="")
    parser.add_argument("--branch", default="", help="Push branch name, for new scratch verification policy")
    parser.add_argument("--event", choices=("push", "workflow_dispatch"), required=True)
    parser.add_argument("--github-output", type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}", args.candidate_revision):
        parser.error("Exact 40-character candidate revision required")
    def git(*arguments):
        return subprocess.check_output(["git", "-C", str(args.candidate), *arguments])
    if git("rev-parse", "HEAD").decode().strip() != args.candidate_revision:
        raise ValueError("Candidate checkout revision mismatch")
    parent = git("rev-parse", "HEAD^1").decode().strip()
    if not re.fullmatch(r"[0-9a-f]{40}", parent):
        raise ValueError("Actual immediate first-parent revision required")
    count = int(git("rev-list", "--first-parent", "--count", "HEAD"))
    before_count, paths = None, []
    if (args.event == "push" and re.fullmatch(r"[0-9a-f]{40}", args.before_revision)
            and args.before_revision != "0" * 40):
        try:
            before = git("rev-parse", "--verify", args.before_revision + "^{commit}").decode().strip()
            history = git("rev-list", "--first-parent", "HEAD").decode().splitlines()
            if before == args.before_revision and before in history:
                previous = int(git("rev-list", "--first-parent", "--count", before))
                if 0 < previous < count:
                    paths = [name.decode("utf-8", "surrogateescape") for name in
                             git("diff", "--name-only", "--no-renames", "-z", before, "HEAD", "--").split(b"\0") if name]
                    before_count = previous
        except (subprocess.CalledProcessError, ValueError):
            before_count, paths = None, []
    new_scratch = (args.event == "push" and args.branch.startswith("scratch-verify-")
                   and args.before_revision == "0" * 40)
    if new_scratch:
        # A newly published verification branch has no prior remote tip. Its
        # immediate-parent tree difference still gives a useful impact signal;
        # a zero before SHA alone must not launch throughput measurements.
        paths = [name.decode("utf-8", "surrogateescape") for name in
                 git("diff", "--name-only", "--no-renames", "-z", parent, "HEAD", "--").split(b"\0") if name]
        before_count = count - 1
    selected, reason = selection(args.event, count, paths, before_count, args.branch)
    if new_scratch:
        reason = "New scratch branch; immediate-parent policy: " + reason
    with args.github_output.open("a") as output:
        output.write(f"run={str(selected).lower()}\nparent_revision={parent}\n")
        output.write(f"commit_count={count}\nreason={reason}\n")
    print(reason)


if __name__ == "__main__":
    main()
