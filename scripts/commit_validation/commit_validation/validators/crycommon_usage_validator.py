#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from typing import List, Type

from commit_validation.commit_validation import Commit, CommitValidator, VERBOSE
from commit_validation.crycommon_usage import SOURCE_PATTERN_MAXIMUMS, line_uses_retired_source

class CryCommonUsageValidator(CommitValidator):
    """Rejects additions to retired CryCommon source and target dependency surfaces."""

    def run(self, commit: Commit, errors: List[str]) -> bool:
        for file_name in commit.get_files():
            normalized_name = file_name.replace('\\', '/')
            source_compatibility_allowlisted = normalized_name in SOURCE_PATTERN_MAXIMUMS
            for line_number, line in enumerate(commit.get_file_diff(file_name).splitlines(), 1):
                if not line.startswith('+') or line.startswith('+++'):
                    continue
                added_line = line[1:]
                # Compatibility declarations and tests are bounded by the companion full-tree
                # check. Keep the commit validator focused on additions everywhere else so an
                # intentional restoration does not reject its own commit.
                retired_source = (
                    not source_compatibility_allowlisted
                    and line_uses_retired_source(added_line, normalized_name)
                )
                new_dependency = 'Legacy::CryCommon' in added_line
                if retired_source or new_dependency:
                    error_message = (
                        f'{file_name}:{line_number}::{self.__class__.__name__} FAILED - '
                        f'new CryCommon usage is not permitted:\n---> {line}'
                    )
                    if VERBOSE:
                        print(error_message)
                    errors.append(error_message)
        return not errors


def get_validator() -> Type[CryCommonUsageValidator]:
    return CryCommonUsageValidator
