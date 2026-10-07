#!/usr/bin/env python3
"""Compare two exact QuickJS revisions using the same frozen workloads.

Hosted measurements are informational. Build failures, corrupt fixtures,
missing scores, timeouts, and workload failures are ordinary errors.
"""

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import statistics
import subprocess
import sys
import time
import urllib.request


HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "manifest.json"


class BenchmarkCancelled(Exception):
    """A termination request handled through normal child cleanup."""


def cancel_on_signal(signum, unused_frame):
    raise BenchmarkCancelled(f"Cancelled by signal {signum}")


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def positive(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("Expected a numeric score")
    if not math.isfinite(value) or value <= 0:
        raise ValueError("Expected a finite positive score")
    return value


def verify_fixture(path, expected):
    if sha256(path) != expected:
        raise ValueError(f"Fixture checksum mismatch: {path}")


def parse_v8(text, suite, case):
    lines = [line[11:] for line in text.splitlines()
             if line.startswith("V8_DETAILS ")]
    if len(lines) != 1 or re.search(r"^(?:ERROR |\*\*\* Run\(\) failed)",
                                  text, re.MULTILINE):
        raise ValueError("Missing, repeated, or failed V8 result")
    data = json.loads(lines[0])
    if len(data) != 1 or data[0]["name"] != suite:
        raise ValueError("Unexpected V8 suite")
    results = data[0]["results"]
    if len(results) != 1 or results[0]["name"] != case:
        raise ValueError("Unexpected V8 benchmark")
    return {f"{suite}/{case}": positive(
        results[0]["microseconds_per_iteration"])}


def parse_micro(path, case, text):
    if re.search(r"^sort_bench: out of order error for ", text, re.MULTILINE):
        raise ValueError("Microbenchmark reported an out-of-order sort result")
    data = json.loads(path.read_text())
    if list(data) != [case]:
        raise ValueError("Unexpected microbenchmark result set")
    return {case: positive(data[case])}


def parse_zoo(text, expected):
    # The pinned Zoo harness parses these original named score lines.
    # Require exactly one numeric line for every expected score and reject
    # errors that the upstream JavaScript runner only prints to stdout.
    result = {}
    for name in expected:
        matches = re.findall(r"^" + re.escape(name) + r": (\S+)\s*$",
                             text, re.MULTILINE)
        if len(matches) != 1:
            raise ValueError(f"Missing or repeated Zoo score: {name}")
        result[name] = positive(float(matches[0]))
    if re.search(r"^(?:ERROR |.*: (?:Error|TypeError|RangeError|Skipped))",
                 text, re.MULTILINE):
        raise ValueError("Zoo workload reported an error")
    return result


def observed_command(command):
    try:
        result = subprocess.run(command, text=True, capture_output=True,
                                timeout=15)
        return {"command": command, "returncode": result.returncode,
                "stdout": result.stdout, "stderr": result.stderr}
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"command": command, "unavailable": str(error)}


def read_optional(path):
    try:
        return Path(path).read_text().strip()
    except OSError as error:
        return {"unavailable": str(error)}


def environment(compiler, affinity):
    policies = {}
    for path in sorted(Path("/sys/devices/system/cpu/cpufreq").glob("policy*")):
        policies[path.name] = {name: read_optional(path / name) for name in (
            "affected_cpus", "scaling_governor", "scaling_cur_freq",
            "scaling_min_freq", "scaling_max_freq", "cpuinfo_min_freq",
            "cpuinfo_max_freq")}
    compiler_path = shutil.which(compiler)
    linker = observed_command([compiler, "-print-prog-name=ld"])
    linker_name = linker.get("stdout", "").strip()
    return {
        "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
        "platform": platform.platform(), "uname": list(platform.uname()),
        "python": sys.version, "os_release": read_optional("/etc/os-release"),
        "cpu_topology": observed_command(["lscpu"]),
        "cpu_info": read_optional("/proc/cpuinfo"),
        "memory": read_optional("/proc/meminfo"),
        "virtualization": observed_command(["systemd-detect-virt"]),
        "affinity": affinity,
        "current_allowed_cpus": sorted(os.sched_getaffinity(0))
            if hasattr(os, "sched_getaffinity") else "unavailable",
        "frequency_policies": policies,
        "cpufreq_boost": read_optional("/sys/devices/system/cpu/cpufreq/boost"),
        "intel_no_turbo": read_optional(
            "/sys/devices/system/cpu/intel_pstate/no_turbo"),
        "aslr": read_optional("/proc/sys/kernel/randomize_va_space"),
        "compiler_path": compiler_path,
        "compiler": observed_command([compiler, "--version"]),
        "linker_selection": linker,
        "linker": observed_command([linker_name, "--version"])
            if linker_name else {"unavailable": "Compiler did not name linker"},
        "make": observed_command(["make", "--version"]),
        "clock": {"process_wall_time": vars(time.get_clock_info("perf_counter")),
                  "v8": "Date milliseconds, original V8 v7 harness",
                  "microbench": "os.now milliseconds, reported ns per operation",
                  "zoo": "Pinned Octane harness score, larger is faster"},
        "runner": {name: os.environ[name] for name in (
            "RUNNER_OS", "RUNNER_ARCH", "ImageOS", "ImageVersion",
            "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT", "GITHUB_SHA")
            if name in os.environ},
        "host_controls": "Turbo, ASLR, governor, and host scheduling unchanged",
        "interpretation": "A shared runner; affinity does not grant exclusivity",
    }


def select_affinity(cpu):
    report = {"requested": cpu, "exclusive": False}
    if cpu == "none":
        report["status"] = "No affinity requested"
        return report
    if not hasattr(os, "sched_getaffinity") or not hasattr(os, "sched_setaffinity"):
        report["status"] = "Affinity unavailable on this platform"
        return report
    allowed = sorted(os.sched_getaffinity(0))
    report["allowed_before"] = allowed
    chosen = allowed[0] if cpu == "auto" else int(cpu)
    if chosen not in allowed:
        raise ValueError(f"Requested CPU {chosen} is outside allowed CPUs")
    try:
        os.sched_setaffinity(0, {chosen})
        report.update(status="Pinned one allowed CPU", selected_cpu=chosen)
    except OSError as error:
        report.update(status="Affinity unavailable", error=str(error))
    return report


def checked_revision(source, expected):
    result = subprocess.run(["git", "-C", str(source), "rev-parse", "HEAD"],
                            text=True, capture_output=True, check=True)
    revision = result.stdout.strip()
    if revision != expected:
        raise ValueError(f"Revision mismatch: {source}: {revision} != {expected}")
    subprocess.run(["git", "-C", str(source), "diff", "--quiet", "HEAD", "--"],
                   check=True)
    return revision


def checked_process(command, cwd, log, timeout):
    started = time.perf_counter()
    with log.open("x") as output:
        process = subprocess.Popen(command, cwd=cwd, stdout=output,
                                   stderr=subprocess.STDOUT,
                                   start_new_session=os.name == "posix")
        try:
            returncode = process.wait(timeout=timeout)
        except BaseException:
            # A timed out build can still have compiler children. Drain its
            # complete process group before returning or raising on POSIX.
            if os.name == "posix":
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            else:
                process.kill()
            process.wait()
            raise
    if returncode:
        raise subprocess.CalledProcessError(returncode, command)
    return time.perf_counter() - started


def require_pristine_build(source):
    # A clean tracked tree alone does not exclude ignored objects produced
    # by a different compiler or configuration. Refuse them without deleting
    # user files. Fresh CI checkouts satisfy this contract automatically.
    tracked = subprocess.check_output(
        ["git", "-C", str(source), "ls-files", "--cached", "-z"])
    tracked = {os.fsdecode(name) for name in tracked.split(b"\0") if name}
    paths = [source / name for name in (
        ".obj", "qjs", "qjs.exe", "qjs-debug", "qjs-debug.exe",
        "qjsc", "qjsc.exe", "host-qjsc", "host-qjsc.exe", "repl.c")]
    paths += [path for pattern in ("*.o", "*.a", "*.d") for path in source.glob(pattern)]
    # These compatibility headers are tracked upstream and generated in the
    # modular tree. Existing tracked originals are legitimate source files.
    paths += [source / name for name in ("quickjs.h", "quickjs-libc.h")
              if name not in tracked]
    stale = sorted({str(path.relative_to(source)) for path in paths
                    if path.exists() or path.is_symlink()})
    if stale:
        raise ValueError("Pristine build required; existing products were left "
                         "untouched: " + ", ".join(stale))


def build(source, compiler, output, jobs):
    require_pristine_build(source)
    command = ["make", f"-j{jobs}", f"CC={compiler}", f"HOST_CC={compiler}",
               "CONFIG_CLANG=" + ("y" if "clang" in compiler else ""),
               "CONFIG_LTO=", "CONFIG_WERROR=y", "qjs"]
    elapsed = checked_process(command, source, output / "build.log", 1200)
    text = (output / "build.log").read_text()
    if "-flto" in text or "-fsanitize=" in text:
        raise ValueError("Unexpected LTO or sanitizer build")
    compiler_lines = [line for line in text.splitlines()
                      if line.startswith(compiler + " ") and
                      re.search(r"(?:^|\s)-c(?:\s|$)", line)]
    if not compiler_lines or any("-O2" not in line for line in compiler_lines):
        raise ValueError("Every compiled runtime object must show -O2")
    if any(re.search(r"(?:^|\s)-O(?:[013szg]|fast)(?:\s|$)", line)
           for line in compiler_lines):
        raise ValueError("Unexpected optimization level in runtime build")
    binary = source / "qjs"
    if not binary.is_file():
        raise ValueError("Build did not produce qjs")
    return {"command": command, "seconds": elapsed, "binary": str(binary),
            "binary_sha256": sha256(binary), "build_log_sha256": sha256(output / "build.log"),
            "actual_compile_commands": compiler_lines,
            "flags": "Makefile defaults with -O2, non-LTO, no sanitizers, Werror"}


def fetch_zoo(manifest, output):
    directory = output / "zoo-fixtures"
    directory.mkdir()
    entries = {**manifest["zoo"]["files"], "LICENSE": {
        "url": manifest["zoo"]["license_url"],
        "sha256": manifest["zoo"]["license_sha256"]}}
    for name, entry in entries.items():
        with urllib.request.urlopen(entry["url"], timeout=60) as response:
            value = response.read(32 * 1024 * 1024 + 1)
        if len(value) > 32 * 1024 * 1024:
            raise ValueError(f"Oversized fixture: {name}")
        path = directory / name
        path.write_bytes(value)
        verify_fixture(path, entry["sha256"])
    return directory


def assemble_v8(manifest, source, output):
    provenance = manifest["v8_provenance"]
    directory = source / provenance["source_directory"]
    order = provenance["assembly_order"]
    for name in order:
        verify_fixture(directory / name, provenance["original_files_sha256"][name])
    # Use the canonical separate originals, not the archive's stale
    # generated combined.js. Keep upstream run_harness.js unchanged.
    value = b"".join((directory / name).read_bytes() for name in order[:-1])
    value += (HERE / provenance["runner"]).read_bytes()
    output.write_bytes(value)
    verify_fixture(output, provenance["assembled_sha256"])
    return output


def summarize(rows, rounds):
    report = {}
    for suite in dict.fromkeys(row["suite"] for row in rows):
        subset = [row for row in rows if row["suite"] == suite]
        cases = {}
        for case in dict.fromkeys(row["case"] for row in subset):
            selected = [row for row in subset if row["case"] == case]
            values = {}
            for variant in ("upstream", "candidate"):
                samples = [row["value"] for row in selected if row["variant"] == variant]
                if len(samples) != rounds[suite]:
                    raise ValueError(f"Incomplete {suite}/{case}/{variant} samples")
                values[variant] = sorted(samples)[len(samples) // 2] if suite == "zoo" else statistics.median(samples)
            ratio = values["candidate"] / values["upstream"] if suite == "zoo" else values["upstream"] / values["candidate"]
            cases[case] = {"upstream": values["upstream"], "candidate": values["candidate"],
                           "sample_count_each": rounds[suite], "unit": selected[0]["unit"],
                           "candidate_speed_ratio": ratio,
                           "candidate_change_percent": 100 * (ratio - 1)}
        aggregate = math.exp(statistics.mean(math.log(case["candidate_speed_ratio"])
                                            for case in cases.values()))
        report[suite] = {"cases": cases, "case_count": len(cases),
                         "candidate_speed_ratio": aggregate,
                         "candidate_change_percent": 100 * (aggregate - 1),
                         "median": "Upper middle sample, matching Zoo" if suite == "zoo" else "Statistical median"}
    return report


def markdown(report, state, observed=None):
    text = ["# QuickJS informational benchmark", "",
            "Shared hosted runner measurements can fluctuate. No performance threshold is a correctness gate.", "",
            f"Status: **{state['status']}**. Compiler: `{state['compiler']}`.", "",
            f"Candidate: `{state['candidate_revision']}`.",
            f"Upstream: `{state['upstream_revision']}`.", "",
            "Both engines use identical frozen workloads, fresh processes per case, -O2, and no LTO.", "",
            "The environment JSON records CPU/topology, affinity, frequency/Turbo availability, virtualization, memory, OS/kernel, compilers/linker, and timing clocks.", ""]
    if observed:
        def one_line(value):
            if not isinstance(value, str):
                value = json.dumps(value)
            return value.replace("|", "\\|").replace("\n", " ")

        cpu_info = observed.get("cpu_info", "")
        model = re.search(r"^model name\s*:\s*(.+)$", cpu_info, re.MULTILINE) if isinstance(cpu_info, str) else None
        compiler = observed.get("compiler", {}).get("stdout", "Unavailable")
        linker = observed.get("linker", {}).get("stdout", "Unavailable")
        memory = observed.get("memory", "")
        total = re.search(r"^MemTotal:\s*(.+)$", memory, re.MULTILINE) if isinstance(memory, str) else None
        text.extend(["## Observed environment", "",
                     "| Item | Value |", "| --- | --- |"])
        for name, value in (
            ("CPU model", model.group(1) if model else "Unavailable"),
            ("CPU topology", observed.get("cpu_topology", {}).get("stdout", "Unavailable")),
            ("Affinity", observed.get("affinity", "Unavailable")),
            ("Allowed CPUs during measurement", observed.get("current_allowed_cpus", "Unavailable")),
            ("Frequency and governor", observed.get("frequency_policies", "Unavailable")),
            ("Turbo controls", {"boost": observed.get("cpufreq_boost"), "intel_no_turbo": observed.get("intel_no_turbo")}),
            ("Virtualization", observed.get("virtualization", {})),
            ("Memory", total.group(1) if total else "Unavailable"),
            ("OS and kernel", observed.get("platform", "Unavailable")),
            ("Compiler", compiler.splitlines()[0] if compiler else "Unavailable"),
            ("Linker", linker.splitlines()[0] if linker else "Unavailable"),
            ("Python", observed.get("python", "Unavailable")),
            ("Clock", observed.get("clock", "Unavailable")),
            ("Host controls", observed.get("host_controls", "Unavailable")),
        ):
            text.append(f"| {name} | {one_line(value)} |")
        text.append("")
    if state.get("error"):
        text.extend([f"Failure: `{state['error']}`", ""])
    for suite, data in report.items():
        text.extend([f"## {suite}", "",
                     f"Geometric mean speed change: **{data['candidate_change_percent']:+.2f}%**. Positive is faster.", "",
                     "| Case | Upstream | Candidate | Unit | Samples each | Change |",
                     "| --- | ---: | ---: | --- | ---: | ---: |"])
        for name, case in data["cases"].items():
            text.append(f"| {name} | {case['upstream']:.6g} | {case['candidate']:.6g} | {case['unit']} | {case['sample_count_each']} | {case['candidate_change_percent']:+.2f}% |")
        text.append("")
    return "\n".join(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--upstream", type=Path, required=True)
    parser.add_argument("--candidate-revision", required=True)
    parser.add_argument("--compiler", choices=("gcc", "clang"), required=True)
    parser.add_argument("--suite", choices=("core", "zoo", "all"), default="core")
    parser.add_argument("--rounds", type=int, choices=(1, 3, 4, 5, 6), default=1,
                        help="V8/micro paired rounds; Zoo always uses 10")
    parser.add_argument("--cpu", default="auto", help="An allowed CPU, auto, or none")
    parser.add_argument("--build-jobs", type=int, default=2)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.build_jobs < 1 or not re.fullmatch(r"[0-9a-f]{40}", args.candidate_revision):
        parser.error("Positive build jobs and exact 40-character revision required")
    args.candidate = args.candidate.resolve()
    args.upstream = args.upstream.resolve()
    args.output = args.output.resolve()
    if args.candidate == args.upstream or args.output.exists():
        parser.error("Distinct source directories and a new output directory required")
    manifest = json.loads(MANIFEST.read_text())
    args.output.mkdir(parents=True)
    state = {"status": "running", "compiler": args.compiler,
             "candidate_revision": args.candidate_revision,
             "upstream_revision": manifest["upstream_revision"],
             "suite": args.suite, "rounds": {"v8": args.rounds, "microbench": args.rounds, "zoo": 10},
             "fresh_process_per_case": True, "manifest_sha256": sha256(MANIFEST)}
    rows = []
    analysis = {}
    observed = None
    previous_sigterm = signal.signal(signal.SIGTERM, cancel_on_signal)
    try:
        write_json(args.output / "state.json", state)
        shutil.copyfile(MANIFEST, args.output / "fixture-manifest.json")
        for name, expected in manifest["bundled_sha256"].items():
            verify_fixture(HERE / "fixtures" / name, expected)
        checked_revision(args.candidate, args.candidate_revision)
        checked_revision(args.upstream, manifest["upstream_revision"])
        v8 = assemble_v8(manifest, args.candidate, args.output / "fresh-v8.js") \
            if args.suite in ("core", "all") else None
        # Capture and build before pinning measurements to one allowed CPU.
        before_affinity = {"status": "Before measurement affinity selection"}
        observed = environment(args.compiler, before_affinity)
        write_json(args.output / "environment-before-build.json", observed)
        builds = {}
        for variant, source in (("upstream", args.upstream), ("candidate", args.candidate)):
            directory = args.output / variant
            directory.mkdir()
            builds[variant] = build(source, args.compiler, directory, args.build_jobs)
            write_json(args.output / "builds.json", builds)
        checked_revision(args.candidate, args.candidate_revision)
        checked_revision(args.upstream, manifest["upstream_revision"])
        zoo = fetch_zoo(manifest, args.output) if args.suite in ("zoo", "all") else None
        cases = []
        if args.suite in ("core", "all"):
            cases.extend(("v8", "/".join(case), case) for case in manifest["v8_cases"])
            cases.extend(("microbench", case, case) for case in manifest["micro_cases"])
        if zoo:
            cases.extend(("zoo", name, value) for name, value in manifest["zoo"]["files"].items())
        affinity = select_affinity(args.cpu)
        observed = environment(args.compiler, affinity)
        write_json(args.output / "environment-before-measurement.json", observed)
        for index, (suite, case, detail) in enumerate(cases):
            for rep in range(state["rounds"][suite]):
                order = ("upstream", "candidate") if (index + rep) % 2 == 0 else ("candidate", "upstream")
                for variant in order:
                    work = args.output / "runs" / suite / case.replace("/", "-") / f"{rep + 1}-{variant}"
                    work.mkdir(parents=True)
                    binary = builds[variant]["binary"]
                    if suite == "v8":
                        command = [binary, str(v8), *detail]
                        unit = "microseconds/iteration"
                    elif suite == "microbench":
                        command = [binary, "--std", str(HERE / "fixtures/fresh-microbench.js"),
                                   "-s", "result.json", case]
                        unit = "ns/operation"
                    else:
                        command = [binary, "--no-unhandled-rejection", "--script", str(zoo / case)]
                        unit = "Zoo score"
                    write_json(work / "command.json", {"argv": command, "cwd": str(work)})
                    elapsed = checked_process(command, work, work / "output.log", 900)
                    text = (work / "output.log").read_text()
                    values = parse_v8(text, *detail) if suite == "v8" else parse_micro(work / "result.json", case, text) if suite == "microbench" else parse_zoo(text, detail["scores"])
                    for name, value in values.items():
                        rows.append({"suite": suite, "case": name, "variant": variant,
                                     "round": rep + 1, "value": value, "unit": unit,
                                     "process_wall_seconds": elapsed, "fresh_process": True})
                    write_json(args.output / "runs.json", rows)
            print(f"{suite} {case}: complete", flush=True)
        for variant in builds:
            verify_fixture(builds[variant]["binary"], builds[variant]["binary_sha256"])
        checked_revision(args.candidate, args.candidate_revision)
        checked_revision(args.upstream, manifest["upstream_revision"])
        for name, expected in manifest["bundled_sha256"].items():
            verify_fixture(HERE / "fixtures" / name, expected)
        if v8:
            verify_fixture(v8, manifest["v8_provenance"]["assembled_sha256"])
        if zoo:
            for name, fixture in manifest["zoo"]["files"].items():
                verify_fixture(zoo / name, fixture["sha256"])
        analysis = summarize(rows, state["rounds"])
        write_json(args.output / "analysis.json", analysis)
        write_json(args.output / "environment-after-measurement.json", environment(args.compiler, affinity))
        state["status"] = "completed"
    except BaseException as error:
        state.update(status="failed", error=f"{type(error).__name__}: {error}")
        raise
    finally:
        try:
            write_json(args.output / "state.json", state)
            (args.output / "summary.md").write_text(markdown(analysis, state, observed))
            with (args.output / "raw-values.csv").open("w", newline="") as output:
                writer = csv.DictWriter(output, fieldnames=("suite", "case", "variant", "round", "value", "unit", "process_wall_seconds", "fresh_process"))
                writer.writeheader()
                writer.writerows(rows)
        finally:
            signal.signal(signal.SIGTERM, previous_sigterm)


if __name__ == "__main__":
    main()
