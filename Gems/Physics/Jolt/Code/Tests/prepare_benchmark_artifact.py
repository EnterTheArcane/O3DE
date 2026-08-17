#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import argparse
import hashlib
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

import compare_provider_benchmarks


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        for chunk in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_git(source_root: Path, *arguments: str) -> bytes:
    result = subprocess.run(
        ("git", *arguments),
        cwd=source_root,
        check=True,
        capture_output=True,
    )
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Add reproducibility and stale-binary metadata to a raw benchmark report."
    )
    parser.add_argument("raw_report", type=Path)
    parser.add_argument("output_report", type=Path)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--provider", choices=compare_provider_benchmarks.PROVIDERS, required=True)
    parser.add_argument("--compiler-id", required=True)
    parser.add_argument("--compiler-version", required=True)
    parser.add_argument("--build-configuration", default="Release")
    parser.add_argument("--cpu-affinity-policy", required=True)
    parser.add_argument("--benchmark-filter", required=True)
    parser.add_argument("--minimum-time", type=float, required=True)
    parser.add_argument("--repetitions", type=int, default=30)
    arguments = parser.parse_args()

    raw_report = arguments.raw_report.resolve()
    output_report = arguments.output_report.resolve()
    binary = arguments.binary.resolve()
    source_root = arguments.source_root.resolve()
    if not raw_report.is_file() or not binary.is_file() or not source_root.is_dir():
        print("The raw report, binary, or source root does not exist.", file=sys.stderr)
        return 1
    if raw_report.stat().st_mtime_ns < binary.stat().st_mtime_ns:
        print("The benchmark report predates its measured binary.", file=sys.stderr)
        return 1
    if arguments.repetitions <= 0 or arguments.minimum_time <= 0.0:
        print("Repetitions and minimum time must be positive.", file=sys.stderr)
        return 1

    with raw_report.open(encoding="utf-8") as report_file:
        report = json.load(report_file)
    if report.get("context", {}).get("library_build_type") != "release":
        print("The raw benchmark report is not from a Release build.", file=sys.stderr)
        return 1

    source_revision = run_git(source_root, "rev-parse", "HEAD").decode("utf-8").strip()
    source_diff = run_git(source_root, "diff", "--binary", "HEAD")
    report["qualification"] = {
        "benchmark_filter": arguments.benchmark_filter,
        "binary_path": str(binary),
        "binary_sha256": sha256_file(binary),
        "build_configuration": arguments.build_configuration,
        "compiler_id": arguments.compiler_id,
        "compiler_version": arguments.compiler_version,
        "cpu_affinity_policy": arguments.cpu_affinity_policy,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "minimum_time": arguments.minimum_time,
        "provider": arguments.provider,
        "raw_samples": True,
        "repetitions": arguments.repetitions,
        "source_diff_sha256": hashlib.sha256(source_diff).hexdigest(),
        "source_revision": source_revision,
        "workload_signature": compare_provider_benchmarks.workload_signature(),
    }

    output_report.parent.mkdir(parents=True, exist_ok=True)
    with output_report.open("w", encoding="utf-8", newline="\n") as output_file:
        json.dump(report, output_file, indent=2)
        output_file.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
