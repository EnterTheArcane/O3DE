#!/usr/bin/env python3
"""Summarize CMake --profiling-format=google-trace output.

This script is designed for very large trace files (hundreds of MB) and streams
JSON events instead of loading the whole file.

Examples:
  python scripts/cmake/profile_cmake_trace.py --build-dir Build/Performance
  python scripts/cmake/profile_cmake_trace.py --profile Build/Performance/cmake-profile-after.json

Open the trace visually with:
  https://ui.perfetto.dev  (or chrome://tracing)
"""

from __future__ import annotations

import argparse
import glob
import heapq
import json
import os
from collections import defaultdict
from dataclasses import dataclass
from typing import DefaultDict, Dict, Iterable, Iterator, List, Optional, Tuple


@dataclass(frozen=True)
class Location:
    file: str
    line: str = ""


def _parse_location(args: object) -> Location:
    if not isinstance(args, dict):
        return Location("")

    loc = args.get("location")
    if isinstance(loc, str) and loc:
        # CMake typically encodes as "path/to/file.cmake:123".
        # Use rsplit to support Windows drive letters.
        if ":" in loc:
            left, right = loc.rsplit(":", 1)
            if right.isdigit():
                return Location(left, right)
        return Location(loc)

    file = args.get("file") or args.get("script")
    line = args.get("line")
    if isinstance(file, str) and file:
        return Location(file, str(line) if line is not None else "")

    return Location("")


def _choose_profile_path(build_dir: str, explicit_profile: Optional[str]) -> str:
    if explicit_profile:
        return explicit_profile

    patterns = [
        os.path.join(build_dir, "cmake-profile-*.json"),
        os.path.join(build_dir, "cmake-profile.json"),
    ]
    candidates: List[str] = []
    for pat in patterns:
        candidates.extend(glob.glob(pat))

    if not candidates:
        raise SystemExit(
            f"No CMake profile json found in '{build_dir}'. "
            "Run CMake with --profiling-format=google-trace --profiling-output=<file>."
        )

    # Most recently modified.
    candidates.sort(key=lambda p: os.path.getmtime(p), reverse=True)
    return candidates[0]


def _stream_events_from_array_text(fp) -> Iterator[dict]:
    dec = json.JSONDecoder()
    buf = ""

    while True:
        if not buf:
            chunk = fp.read(1 << 20)
            if not chunk:
                return
            buf += chunk

        i = 0
        while i < len(buf) and buf[i] in " \t\r\n,":
            i += 1
        buf = buf[i:]

        if not buf:
            continue

        if buf[0] == "]":
            return

        try:
            obj, idx = dec.raw_decode(buf)
        except ValueError:
            chunk = fp.read(1 << 20)
            if not chunk:
                return
            buf += chunk
            continue

        buf = buf[idx:]
        if isinstance(obj, dict):
            yield obj


def stream_trace_events(path: str) -> Iterator[dict]:
    """Yield trace events from a google-trace file.

    CMake 4.2+ currently emits a top-level JSON array.
    Some tools emit {"traceEvents": [...]}.
    """

    with open(path, "r", encoding="utf-8") as f:
        buf = ""

        while True:
            chunk = f.read(1 << 20)
            if not chunk:
                raise RuntimeError("Could not find trace event array")
            buf += chunk

            j = 0
            while j < len(buf) and buf[j] in " \t\r\n":
                j += 1

            if j < len(buf) and buf[j] == "[":
                # Top-level array
                # Put the remainder back into a pseudo-file by continuing streaming from f with buf preloaded.
                # We implement this by consuming from a small wrapper generator.
                remainder = buf[j + 1 :]

                def gen() -> Iterator[dict]:
                    # yield from the remainder first
                    for e in _stream_events_from_array_text(_PreloadedReader(f, remainder)):
                        yield e

                yield from gen()
                return

            # Wrapped object
            key_pos = buf.find('"traceEvents"')
            if key_pos != -1:
                arr_pos = buf.find("[", key_pos)
                if arr_pos != -1:
                    remainder = buf[arr_pos + 1 :]

                    def gen() -> Iterator[dict]:
                        for e in _stream_events_from_array_text(_PreloadedReader(f, remainder)):
                            yield e

                    yield from gen()
                    return

            if len(buf) > (1 << 22):
                buf = buf[-(1 << 22) :]


class _PreloadedReader:
    """File-like reader that yields a preloaded prefix before reading the underlying file."""

    def __init__(self, fp, prefix: str):
        self._fp = fp
        self._prefix = prefix

    def read(self, size: int) -> str:
        if self._prefix:
            chunk = self._prefix[:size]
            self._prefix = self._prefix[size:]
            return chunk
        return self._fp.read(size)


def summarize(profile_path: str, *, top_events: int, top_files: int, top_names: int, progress_every: int) -> None:
    total_events = 0
    total_x = 0
    total_be_pairs = 0

    # (pid, tid) -> begin stack
    stacks: DefaultDict[Tuple[int, int], List[dict]] = defaultdict(list)

    top: List[Tuple[float, str, str, str]] = []  # (dur_us, name, file, line)
    by_file: DefaultDict[str, float] = defaultdict(float)
    by_name: DefaultDict[str, float] = defaultdict(float)

    for e in stream_trace_events(profile_path):
        total_events += 1
        ph = e.get("ph")

        if ph == "X":
            dur = e.get("dur")
            if not isinstance(dur, (int, float)):
                continue
            total_x += 1

            name = str(e.get("name") or "")
            loc = _parse_location(e.get("args"))

            by_name[name] += float(dur)
            if loc.file:
                by_file[loc.file] += float(dur)

            item = (float(dur), name, loc.file, loc.line)
            if len(top) < top_events:
                heapq.heappush(top, item)
            else:
                if item[0] > top[0][0]:
                    heapq.heapreplace(top, item)

        elif ph == "B":
            pid = e.get("pid")
            tid = e.get("tid")
            ts = e.get("ts")
            if not isinstance(pid, int) or not isinstance(tid, int) or not isinstance(ts, (int, float)):
                continue
            stacks[(pid, tid)].append(e)

        elif ph == "E":
            pid = e.get("pid")
            tid = e.get("tid")
            ts_end = e.get("ts")
            if not isinstance(pid, int) or not isinstance(tid, int) or not isinstance(ts_end, (int, float)):
                continue
            st = stacks.get((pid, tid))
            if not st:
                continue
            begin = st.pop()
            ts_begin = begin.get("ts")
            if not isinstance(ts_begin, (int, float)):
                continue

            dur = float(ts_end) - float(ts_begin)
            if dur < 0:
                continue
            total_be_pairs += 1

            name = str(begin.get("name") or "")
            loc = _parse_location(begin.get("args"))

            by_name[name] += dur
            if loc.file:
                by_file[loc.file] += dur

            item = (dur, name, loc.file, loc.line)
            if len(top) < top_events:
                heapq.heappush(top, item)
            else:
                if item[0] > top[0][0]:
                    heapq.heapreplace(top, item)

        if progress_every and total_events % progress_every == 0:
            print(f"...parsed {total_events:,} events")

    print(f"Profile: {profile_path}")
    print(f"Parsed events: {total_events:,}  complete(X): {total_x:,}  B/E pairs: {total_be_pairs:,}")

    print(f"\nTop {top_events} events by duration (ms):")
    for dur, name, file, line in sorted(top, key=lambda t: t[0], reverse=True):
        hint = f" @ {file}:{line}" if file else ""
        print(f"{dur/1000.0:10.1f}  {name}{hint}")

    print(f"\nTop {top_files} files by total duration (s):")
    for fpath, dur in sorted(by_file.items(), key=lambda kv: kv[1], reverse=True)[:top_files]:
        print(f"{dur/1e6:10.2f}  {fpath}")

    print(f"\nTop {top_names} names by total duration (s):")
    for nm, dur in sorted(by_name.items(), key=lambda kv: kv[1], reverse=True)[:top_names]:
        print(f"{dur/1e6:10.2f}  {nm}")


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Summarize CMake google-trace profiling output")
    p.add_argument("--profile", help="Path to google-trace json produced by CMake")
    p.add_argument("--build-dir", default=os.path.join("Build", "Performance"), help="Build directory to search for a cmake-profile*.json")
    p.add_argument("--top-events", type=int, default=25)
    p.add_argument("--top-files", type=int, default=25)
    p.add_argument("--top-names", type=int, default=25)
    p.add_argument("--progress-every", type=int, default=300_000, help="Print progress every N events (0 disables)")
    return p.parse_args()


def main() -> None:
    ns = parse_args()
    profile_path = _choose_profile_path(ns.build_dir, ns.profile)
    summarize(
        profile_path,
        top_events=ns.top_events,
        top_files=ns.top_files,
        top_names=ns.top_names,
        progress_every=ns.progress_every,
    )


if __name__ == "__main__":
    main()
