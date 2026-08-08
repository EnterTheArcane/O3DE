#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Generate a review manifest for every hunk in the CryCommon retirement diff."""

import argparse
import csv
import re
import subprocess
from pathlib import Path


HUNK_PATTERN = re.compile(r'^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@')
FOUNDATIONAL_COMPATIBILITY_FILES = {
    'Code/Legacy/CryCommon/CryRandomInternal.h',
    'Code/Legacy/CryCommon/Cry_Color.h',
    'Code/Legacy/CryCommon/Cry_Math.h',
    'Code/Legacy/CryCommon/Cry_Matrix33.h',
    'Code/Legacy/CryCommon/Cry_Matrix34.h',
    'Code/Legacy/CryCommon/Cry_Matrix44.h',
    'Code/Legacy/CryCommon/Cry_Quat.h',
    'Code/Legacy/CryCommon/Cry_Vector2.h',
    'Code/Legacy/CryCommon/Cry_Vector3.h',
    'Code/Legacy/CryCommon/Cry_Vector4.h',
    'Code/Legacy/CryCommon/LCGRandom.h',
    'Code/Legacy/CryCommon/MathConversion.h',
}


def run_git(repository_root: Path, *arguments: str) -> str:
    return subprocess.run(
        ('git', *arguments), cwd=repository_root, check=True, text=True, stdout=subprocess.PIPE
    ).stdout


def classify(path: str) -> tuple[str, str, str]:
    if path in FOUNDATIONAL_COMPATIBILITY_FILES:
        return (
            'compatibility',
            'Restore the exported Cry math/random source-compatibility contract.',
            'restore',
        )
    if path.endswith(('CMakeLists.txt', '.cmake')):
        return ('build dependencies', 'Remove only dependencies proven unused by no-unity compilation.', 'accept')
    if 'Serialization' in path or 'Serialize' in path or 'AnimSplineTrack' in path or 'AzEntityNode' in path:
        return ('serialization', 'Preserve reflected ids, legacy conversion behavior, and semantic asset values.', 'accept')
    if 'VertexFormats' in path or 'UiRenderFormats' in path or 'FFont' in path or path.endswith('/Draw2d.cpp'):
        return ('layout/fonts', 'Preserve CPU/GPU size, alignment, offsets, strides, bytes, and text output.', 'accept')
    if 'TrackView' in path or '/Animation/' in path or '/Cinematics/' in path:
        return ('animation/math', 'Preserve animation ordering, key data, and legacy numeric edge behavior.', 'accept')
    if path.startswith('scripts/commit_validation/'):
        return ('validation guard', 'Prevent the retired source/dependency surface from increasing.', 'accept')
    if path.endswith(('.h', '.cpp', '.inl')):
        return ('consumers', 'Use an equivalent AZ/AZStd contract or remove a proven dead dependency.', 'accept')
    return ('other', 'Preserve the observable baseline contract.', 'accept')


def parse_diff(diff: str, ignored_paths: set[str] | None = None) -> list[dict[str, object]]:
    ignored_paths = ignored_paths or set()
    rows = []
    path = ''
    old_path = ''
    hunk_index = 0
    for line in diff.splitlines():
        if line.startswith('diff --git '):
            # Never carry the preceding file into a new/deleted file whose +++
            # marker is /dev/null.
            path = ''
            old_path = ''
            hunk_index = 0
            continue
        if line.startswith('--- a/'):
            old_path = line[6:]
            continue
        if line.startswith('+++ b/'):
            path = line[6:]
            hunk_index = 0
            continue
        if line == '+++ /dev/null':
            path = old_path
            hunk_index = 0
            continue
        match = HUNK_PATTERN.match(line)
        if not match or not path or path in ignored_paths:
            continue
        hunk_index += 1
        group, claim, disposition = classify(path)
        rows.append(
            {
                'hunk_id': f'{path}#{hunk_index}',
                'group': group,
                'path': path,
                'old_start': int(match.group(1)),
                'old_count': int(match.group(2) or 1),
                'new_start': int(match.group(3)),
                'new_count': int(match.group(4) or 1),
                'claim': claim,
                'evidence': 'See CryCommonMigrationValidation.md and category tests.',
                'disposition': disposition,
                'legacy_review': 'pending frozen-SHA signoff',
                'az_review': 'pending frozen-SHA signoff',
            }
        )
    return rows


def find_base_equivalent_untracked_files(repository_root: Path, base: str) -> set[str]:
    equivalent_paths = set()
    for path in run_git(repository_root, 'ls-files', '--others', '--exclude-standard').splitlines():
        absolute_path = repository_root / path
        if not absolute_path.is_file():
            continue
        base_file = subprocess.run(
            ('git', 'show', f'{base}:{path}'),
            cwd=repository_root,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        if base_file.returncode == 0 and base_file.stdout == absolute_path.read_bytes():
            equivalent_paths.add(path)
    return equivalent_paths


def add_untracked_files(
    repository_root: Path,
    rows: list[dict[str, object]],
    ignored_paths: set[str],
) -> None:
    untracked = run_git(repository_root, 'ls-files', '--others', '--exclude-standard').splitlines()
    for path in untracked:
        if path in ignored_paths or path.startswith(('build/', 'Cache/')):
            continue
        absolute_path = repository_root / path
        if not absolute_path.is_file():
            continue
        line_count = len(absolute_path.read_text(encoding='utf8', errors='replace').splitlines())
        group, claim, disposition = classify(path)
        rows.append(
            {
                'hunk_id': f'{path}#1',
                'group': group,
                'path': path,
                'old_start': 0,
                'old_count': 0,
                'new_start': 1,
                'new_count': line_count,
                'claim': claim,
                'evidence': 'See CryCommonMigrationValidation.md and category tests.',
                'disposition': disposition,
                'legacy_review': 'pending frozen-SHA signoff',
                'az_review': 'pending frozen-SHA signoff',
            }
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', default='upstream/development')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()

    repository_root = Path(__file__).resolve().parents[2]
    output_path = args.output if args.output.is_absolute() else repository_root / args.output
    output_relative_path = output_path.relative_to(repository_root).as_posix()
    base_equivalent_paths = find_base_equivalent_untracked_files(repository_root, args.base)
    diff = run_git(repository_root, 'diff', '--no-ext-diff', '--unified=0', '--no-renames', args.base, '--', '.')
    ignored_paths = base_equivalent_paths | {output_relative_path}
    rows = parse_diff(diff, ignored_paths)
    add_untracked_files(repository_root, rows, ignored_paths)
    rows.sort(key=lambda row: (str(row['path']), int(row['new_start']), str(row['hunk_id'])))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open('w', newline='', encoding='utf8') as output_file:
        writer = csv.DictWriter(output_file, fieldnames=rows[0].keys() if rows else ('hunk_id',))
        writer.writeheader()
        writer.writerows(rows)
    print(f'Wrote {len(rows)} hunk records to {output_path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
