#!/usr/bin/env python3
"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Shader product-cache parity verification (SlangIntegrationPlan.md, Phase 1A.4).

Verifies that a shader-pipeline refactor produced byte-identical shader-derived products by
comparing two clean-cache AssetProcessorBatch runs (a baseline checkout and the refactored tree).
The only allowed deltas are AssetProcessor job fingerprints (builder version bumps); product
bytes must match exactly.

Usage:
  # After a clean-cache AssetProcessorBatch run on each side:
  CacheParityDiff.py snapshot --project-path <path-to-project> --output baseline.json
  CacheParityDiff.py snapshot --project-path <path-to-project> --output current.json
  CacheParityDiff.py diff --baseline baseline.json --current current.json

Exit code 0 = GO (byte parity holds), 1 = NO-GO or error.
"""

import argparse
import hashlib
import json
import re
import sqlite3
import sys
from pathlib import Path

# Product name endings that identify shader-derived products, including the cached
# language-backend sub-products fetched by downstream builders.
SHADER_PRODUCT_ENDINGS = (
    ".azshader",
    ".azshadervariant",
    ".azshadervarianttree",
    ".azslin",
    ".ia.json",
    ".om.json",
    ".srg.json",
    ".options.json",
    ".bindingdep.json",
    ".hlsl",
    ".hashedvariantlist",
    ".hashedvariantinfo",
)

# Source name endings whose asset-database rows are captured (products + job fingerprints).
SHADER_SOURCE_ENDINGS = (
    ".shader",
    ".shadervariantlist",
    ".hashedvariantlist",
    ".hashedvariantinfo",
)


# The AssetProcessor job temp directory name is randomized per run and gets embedded in product
# "source" path fields, so it can never byte-match between two runs. Masking it is the only
# normalization applied before hashing; everything else must match exactly. Both runs must execute
# from the same absolute checkout path (use a directory junction when comparing two checkouts),
# because products also embed source paths, which are deliberately NOT normalized.
JOB_TEMP_PATTERN = re.compile(rb"JobTemp-[A-Za-z0-9]+")


def sha256_of(path: Path) -> str:
    content = path.read_bytes()
    content = JOB_TEMP_PATTERN.sub(b"JobTemp-NORMALIZED", content)
    return hashlib.sha256(content).hexdigest()


def find_asset_database(project_path: Path) -> Path | None:
    cache_database = project_path / "Cache" / "assetdb.sqlite"
    if cache_database.is_file():
        return cache_database
    user_dir = project_path / "user"
    if not user_dir.is_dir():
        return None
    candidates = sorted(user_dir.rglob("assetdb.sqlite"))
    return candidates[0] if candidates else None


def snapshot_products(cache_root: Path) -> dict:
    products = {}
    for path in cache_root.rglob("*"):
        if not path.is_file():
            continue
        name = path.name.lower()
        if not name.endswith(SHADER_PRODUCT_ENDINGS):
            continue
        relative = path.relative_to(cache_root).as_posix().lower()
        products[relative] = {
            "sha256": sha256_of(path),
            "size": path.stat().st_size,
        }
    return products


def snapshot_database(database_path: Path) -> dict:
    connection = sqlite3.connect(f"file:{database_path}?mode=ro", uri=True)
    try:
        cursor = connection.cursor()
        rows = cursor.execute(
            """
            SELECT Sources.SourceName, Jobs.JobKey, Jobs.Platform, Jobs.Fingerprint,
                   Products.ProductName, Products.SubID, Products.AssetType
            FROM Sources
            JOIN Jobs ON Jobs.SourcePK = Sources.SourceID
            JOIN Products ON Products.JobPK = Jobs.JobID
            ORDER BY Sources.SourceName, Jobs.JobKey, Products.SubID
            """
        ).fetchall()
    finally:
        connection.close()

    entries = {}
    for source_name, job_key, platform, fingerprint, product_name, sub_id, asset_type in rows:
        if not source_name.lower().endswith(SHADER_SOURCE_ENDINGS):
            continue
        key = f"{source_name.lower()}|{job_key}|{platform}|{sub_id}"
        entries[key] = {
            "product": product_name.lower(),
            "assetType": asset_type.hex() if isinstance(asset_type, bytes) else asset_type,
            "fingerprint": fingerprint,
        }
    return entries


def command_snapshot(arguments) -> int:
    project_path = Path(arguments.project_path).resolve()
    cache_root = project_path / "Cache"
    if not cache_root.is_dir():
        print(f"error: no Cache directory under {project_path}", file=sys.stderr)
        return 1

    snapshot = {
        "projectPath": str(project_path),
        "products": snapshot_products(cache_root),
    }

    database_path = find_asset_database(project_path)
    if database_path:
        snapshot["databaseRows"] = snapshot_database(database_path)
    else:
        print("warning: assetdb.sqlite not found; database rows not captured", file=sys.stderr)
        snapshot["databaseRows"] = {}

    output_path = Path(arguments.output)
    output_path.write_text(json.dumps(snapshot, indent=1, sort_keys=True))
    print(f"snapshot: {len(snapshot['products'])} shader-derived products, "
          f"{len(snapshot['databaseRows'])} database rows -> {output_path}")
    return 0


def command_diff(arguments) -> int:
    baseline = json.loads(Path(arguments.baseline).read_text())
    current = json.loads(Path(arguments.current).read_text())

    baseline_products = baseline["products"]
    current_products = current["products"]

    missing = sorted(set(baseline_products) - set(current_products))
    added = sorted(set(current_products) - set(baseline_products))
    changed = sorted(
        path for path in set(baseline_products) & set(current_products)
        if baseline_products[path]["sha256"] != current_products[path]["sha256"]
    )

    for path in missing:
        print(f"MISSING  {path}")
    for path in added:
        print(f"ADDED    {path}")
    for path in changed:
        print(f"CHANGED  {path}  {baseline_products[path]['size']}B -> {current_products[path]['size']}B")

    fingerprint_only = 0
    row_mismatches = []
    baseline_rows = baseline.get("databaseRows", {})
    current_rows = current.get("databaseRows", {})
    for key in sorted(set(baseline_rows) & set(current_rows)):
        baseline_row = baseline_rows[key]
        current_row = current_rows[key]
        if baseline_row["product"] != current_row["product"] or baseline_row["assetType"] != current_row["assetType"]:
            row_mismatches.append(key)
        elif baseline_row["fingerprint"] != current_row["fingerprint"]:
            fingerprint_only += 1  # allowed: builder version bumps change fingerprints
    for key in sorted(set(baseline_rows) ^ set(current_rows)):
        row_mismatches.append(key)
    for key in row_mismatches:
        print(f"ROW      {key}")

    print(
        f"\nsummary: {len(baseline_products)} baseline products, {len(current_products)} current products; "
        f"{len(changed)} changed, {len(missing)} missing, {len(added)} added; "
        f"{fingerprint_only} fingerprint-only row deltas (allowed), {len(row_mismatches)} row mismatches"
    )

    if changed or missing or added or row_mismatches:
        print("NO-GO: shader product parity broken")
        return 1
    print("GO: shader products are byte-identical")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    subparsers = parser.add_subparsers(dest="command", required=True)

    snapshot_parser = subparsers.add_parser("snapshot", help="capture shader-derived product hashes and asset database rows")
    snapshot_parser.add_argument("--project-path", required=True, help="project root containing Cache/ and user/")
    snapshot_parser.add_argument("--output", required=True, help="snapshot JSON to write")
    snapshot_parser.set_defaults(handler=command_snapshot)

    diff_parser = subparsers.add_parser("diff", help="compare two snapshots")
    diff_parser.add_argument("--baseline", required=True)
    diff_parser.add_argument("--current", required=True)
    diff_parser.set_defaults(handler=command_diff)

    arguments = parser.parse_args()
    return arguments.handler(arguments)


if __name__ == "__main__":
    sys.exit(main())
