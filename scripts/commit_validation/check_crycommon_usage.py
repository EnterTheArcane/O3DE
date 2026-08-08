#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import sys
from pathlib import Path

SCRIPT_DIRECTORY = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIRECTORY))

from commit_validation.crycommon_usage import scan_tree  # noqa: E402


def main() -> int:
    repository_root = SCRIPT_DIRECTORY.parents[1]
    errors = scan_tree(repository_root)
    for error in errors:
        print(error)
    if errors:
        print(f'CryCommon full-tree check failed with {len(errors)} error(s).')
        return 1
    print('CryCommon full-tree check passed.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
