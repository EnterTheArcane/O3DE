#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import argparse
import json
import statistics
import subprocess
import tempfile
import time
from pathlib import Path


def measure(command, iterations):
    subprocess.run(command, check=True, capture_output=True, encoding="utf-8", timeout=120)
    samples = []
    for _ in range(iterations):
        start = time.perf_counter()
        result = subprocess.run(command, check=True, capture_output=True, encoding="utf-8", timeout=120)
        sample = json.loads(result.stdout)
        sample["process_ms"] = (time.perf_counter() - start) * 1000
        samples.append(sample)
    return {
        "median": {
            key: statistics.median(sample[key] for sample in samples)
            for key in samples[0] if isinstance(samples[0][key], (int, float))
        },
        "samples": samples,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--iterations", type=int, default=5)
    parser.add_argument("--counts", type=int, nargs="+", default=[4000, 8000, 16000])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.iterations < 1 or any(count < 1 for count in args.counts):
        parser.error("iterations and counts must be positive")
    results = {}
    with tempfile.TemporaryDirectory(prefix="azsl-benchmark-") as directory:
        for count in args.counts:
            declarations = [f"float value{i};" for i in range(count)]
            cases = {
                "ordinary": "\n".join(declarations),
                "ascii_line": " ".join(declarations),
                "unicode_line": " /*é😀*/ ".join(declarations),
                "macros": "#define BODY(x) float x;\n" + "\n".join(f"BODY(value{i})" for i in range(count)),
            }
            for name, text in cases.items():
                source = Path(directory) / f"{name}_{count}.azsl"
                source.write_text(text + "\n", encoding="utf-8")
                result = measure([str(args.benchmark.resolve()), str(source)], args.iterations)
                if args.compiler:
                    # Includes process launch and dependency serialization, with no parser work.
                    result["dependencies"] = measure(
                        [str(args.compiler.resolve()), str(source), "--dependencies"], args.iterations)
                results[f"{name}_{count}"] = result
                print(name, count, json.dumps(result["median"]), flush=True)
    args.output.write_text(json.dumps(results, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
