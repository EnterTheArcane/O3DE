#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SCRIPT_DIRECTORY = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIRECTORY))

from commit_validation.crycommon_ledger import (  # noqa: E402
    build_ledger,
    csv_text,
    report_text,
    validation_errors,
)


DEFAULT_CSV = Path('Code/Legacy/CryCommon/crycommon_declaration_use_ledger.csv')
DEFAULT_REPORT = Path('Code/Legacy/CryCommon/crycommon_declaration_use_ledger.md')


def _write_or_check(path: Path, content: str, check: bool) -> bool:
    encoded = content.encode('utf8')
    if check:
        if not path.is_file() or path.read_bytes() != encoded:
            print(f'CryCommon ledger artifact is stale or missing: {path}')
            return False
        return True
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encoded)
    print(f'Wrote {path}')
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description='Generate the CryCommon declaration/use ledger.')
    parser.add_argument(
        '--repository-root', type=Path, default=SCRIPT_DIRECTORY.parents[1],
        help='O3DE repository root (defaults to the root containing this script).'
    )
    parser.add_argument('--output-csv', type=Path, default=DEFAULT_CSV)
    parser.add_argument('--output-report', type=Path, default=DEFAULT_REPORT)
    parser.add_argument(
        '--check', action='store_true',
        help='Fail if the checked-in artifacts differ from generated output.'
    )
    arguments = parser.parse_args()

    repository_root = arguments.repository_root.resolve()
    csv_path = arguments.output_csv if arguments.output_csv.is_absolute() else repository_root / arguments.output_csv
    report_path = (
        arguments.output_report
        if arguments.output_report.is_absolute()
        else repository_root / arguments.output_report
    )

    result = build_ledger(repository_root)
    errors = validation_errors(result)
    if errors:
        for error in errors:
            print(f'CryCommon ledger validation error: {error}')
        return 1
    success = _write_or_check(csv_path, csv_text(result), arguments.check)
    success = _write_or_check(report_path, report_text(result), arguments.check) and success
    if success and arguments.check:
        print('CryCommon ledger artifacts are current.')
    return 0 if success else 1


if __name__ == '__main__':
    raise SystemExit(main())
