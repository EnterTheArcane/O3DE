#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import os
import re
from typing import List, Type

from commit_validation.commit_validation import Commit, CommitValidator, SOURCE_FILE_EXTENSIONS, VERBOSE


# These files intentionally expose specific trait-shaped compatibility APIs whose removal would break downstream code.
COMPATIBILITY_ALLOWLIST = {
    'Code/Framework/AzCore/AzCore/std/typetraits/conditional.h': {'enable_if'},
    'Code/Framework/AzCore/AzCore/std/typetraits/conjunction.h': {'boolean trait composition'},
    'Code/Framework/AzCore/AzCore/std/typetraits/disjunction.h': {'boolean trait composition'},
    'Code/Framework/AzCore/AzCore/std/typetraits/negation.h': {'boolean trait composition'},
    'Code/Framework/AzCore/AzCore/std/typetraits/typetraits.h': {'boolean trait composition'},
    'Code/Framework/AzCore/AzCore/std/typetraits/void_t.h': {'void_t detection'},
    'Code/Framework/AzCore/AzCore/DOM/DomUtils.h': {'bool_constant'},
    'Code/Framework/AzCore/AzCore/Preprocessor/Enum.h': {'bool_constant'},
    'Code/Framework/AzCore/AzCore/RTTI/RTTIMacros.h': {'bool_constant'},
    'Code/Framework/AzCore/AzCore/RTTI/TypeInfo.h': {'bool_constant'},
    'Code/Framework/AzCore/AzCore/std/function/invoke.h': {'bool_constant'},
    'Code/Framework/AzCore/AzCore/std/hash.h': {'bool_constant'},
    'Code/Framework/AzCore/AzCore/std/typetraits/internal/is_complete.h': {'bool_constant'},
    'Code/Framework/AzCore/AzCore/std/typetraits/is_function.h': {'bool_constant'},
}

FIRST_PARTY_ROOTS = ('Code/', 'Gems/')
CPP_EXTENSIONS = tuple(extension for extension in SOURCE_FILE_EXTENSIONS if extension not in ('.cs', '.java'))

LEGACY_CONSTRAINT_PATTERNS = (
    ('enable_if', re.compile(r'\b(?:AZStd::|std::)?enable_if(?:_t)?\b')),
    ('void_t detection', re.compile(r'\b(?:AZStd::|std::)?void_t\s*<')),
    ('boolean trait composition', re.compile(
        r'\b(?:AZStd::|std::)?(?:conjunction|disjunction|negation)(?:_v)?\b')),
    ('bool_constant', re.compile(r'\b(?:AZStd::|std::)?bool_constant\s*<')),
    ('custom SFINAE helper', re.compile(
        r'\b(?:sp_enable_if[A-Za-z0-9_]*|[A-Za-z0-9_]+_sfinae[A-Za-z0-9_]*|[A-Za-z0-9_]*sfinae_[A-Za-z0-9_]+)\b',
        re.IGNORECASE)),
)

CONCEPT_DEFINITION_PATTERN = re.compile(r'^\+\s*concept\s+([A-Za-z_][A-Za-z0-9_]*)\b')
AZSTD_PATH_PREFIX = 'Code/Framework/AzCore/AzCore/std/'
AZSTD_CONCEPT_NAME_PATTERN = re.compile(r'^[a-z][a-z0-9_]*$')
O3DE_CONCEPT_NAME_PATTERN = re.compile(r'^[A-Z][A-Za-z0-9]*$')


class Cpp20ConstraintValidator(CommitValidator):
    """Rejects newly added first-party uses of legacy template constraint machinery."""

    def run(self, commit: Commit, errors: List[str]) -> bool:
        for file_name in commit.get_files():
            normalized_name = file_name.replace('\\', '/').lstrip('./')
            if not normalized_name.startswith(FIRST_PARTY_ROOTS):
                continue
            if os.path.splitext(normalized_name)[1].lower() not in CPP_EXTENSIONS:
                continue
            allowed_legacy_forms = COMPATIBILITY_ALLOWLIST.get(normalized_name, set())

            for line_number, line in enumerate(commit.get_file_diff(file_name).splitlines(), start=1):
                if not line.startswith('+') or line.startswith('+++'):
                    continue

                for description, pattern in LEGACY_CONSTRAINT_PATTERNS:
                    match = pattern.search(line)
                    if not match:
                        continue
                    if description in allowed_legacy_forms:
                        continue

                    error_message = (
                        f'{file_name}:{line_number}::{self.__class__.__name__} FAILED - '
                        f'Added legacy C++ constraint machinery ({description}) "{match.group(0)}". '
                        'Use a C++20 concept, requires-expression, requires-clause, or boolean fold expression instead:\n'
                        f'---> {line}'
                    )
                    if VERBOSE:
                        print(error_message)
                    errors.append(error_message)
                    break

                concept_match = CONCEPT_DEFINITION_PATTERN.search(line)
                if concept_match:
                    concept_name = concept_match.group(1)
                    expected_pattern = (
                        AZSTD_CONCEPT_NAME_PATTERN if normalized_name.startswith(AZSTD_PATH_PREFIX)
                        else O3DE_CONCEPT_NAME_PATTERN
                    )
                    if not expected_pattern.fullmatch(concept_name):
                        expected_style = 'lower_snake_case' if normalized_name.startswith(AZSTD_PATH_PREFIX) else 'PascalCase'
                        error_message = (
                            f'{file_name}:{line_number}::{self.__class__.__name__} FAILED - '
                            f'Concept "{concept_name}" must use {expected_style} naming in its owning namespace:\n'
                            f'---> {line}'
                        )
                        if VERBOSE:
                            print(error_message)
                        errors.append(error_message)

        return not errors


def get_validator() -> Type[Cpp20ConstraintValidator]:
    """Returns the validator class for this module."""
    return Cpp20ConstraintValidator
