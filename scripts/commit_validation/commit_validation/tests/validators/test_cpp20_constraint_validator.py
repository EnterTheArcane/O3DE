#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import unittest

from commit_validation.tests.mocks.mock_commit import MockCommit
from commit_validation.validators.cpp20_constraint_validator import Cpp20ConstraintValidator


class Cpp20ConstraintValidatorTests(unittest.TestCase):
    def test_modern_constraints_pass(self):
        commit = MockCommit(
            files=['Code/Framework/Example/Example.h'],
            file_diffs={'Code/Framework/Example/Example.h':
                '+template<class T>\n'
                '+    requires AZStd::integral<T>\n'
                '+void Visit(T value);\n'})

        errors = []
        self.assertTrue(Cpp20ConstraintValidator().run(commit, errors))
        self.assertEqual([], errors)

    def test_legacy_constraint_forms_fail(self):
        legacy_lines = (
            '+AZStd::enable_if_t<Condition, int> value;\n',
            '+using Detected = AZStd::void_t<typename T::type>;\n',
            '+constexpr bool Valid = AZStd::conjunction_v<A, B>;\n',
            '+using Result = AZStd::bool_constant<Condition>;\n',
            '+struct custom_sfinae_helper {};\n',
        )

        for legacy_line in legacy_lines:
            with self.subTest(legacy_line=legacy_line):
                commit = MockCommit(
                    files=['Gems/Example/Code/Example.h'],
                    file_diffs={'Gems/Example/Code/Example.h': legacy_line})
                errors = []
                self.assertFalse(Cpp20ConstraintValidator().run(commit, errors))
                self.assertEqual(1, len(errors))

    def test_removed_and_context_lines_pass(self):
        commit = MockCommit(
            files=['Code/Framework/Example/Example.h'],
            file_diffs={'Code/Framework/Example/Example.h':
                '-using Old = AZStd::enable_if_t<Condition>;\n'
                ' using Existing = AZStd::void_t<typename T::type>;\n'})

        errors = []
        self.assertTrue(Cpp20ConstraintValidator().run(commit, errors))
        self.assertEqual([], errors)

    def test_external_and_non_source_files_pass(self):
        commit = MockCommit(
            files=['External/Library/Legacy.h', 'Code/Framework/Example/notes.txt'],
            file_diffs={
                'External/Library/Legacy.h': '+using Old = std::enable_if_t<Condition>;\n',
                'Code/Framework/Example/notes.txt': '+AZStd::enable_if_t is documented here.\n',
            })

        errors = []
        self.assertTrue(Cpp20ConstraintValidator().run(commit, errors))
        self.assertEqual([], errors)

    def test_compatibility_header_passes(self):
        compatibility_header = 'Code/Framework/AzCore/AzCore/std/typetraits/conditional.h'
        commit = MockCommit(
            files=[compatibility_header],
            file_diffs={compatibility_header: '+using std::enable_if_t;\n'})

        errors = []
        self.assertTrue(Cpp20ConstraintValidator().run(commit, errors))
        self.assertEqual([], errors)

    def test_compatibility_allowlist_is_token_specific(self):
        compatibility_header = 'Code/Framework/AzCore/AzCore/std/typetraits/conditional.h'
        commit = MockCommit(
            files=[compatibility_header],
            file_diffs={compatibility_header: '+using Detected = AZStd::void_t<typename T::type>;\n'})

        errors = []
        self.assertFalse(Cpp20ConstraintValidator().run(commit, errors))
        self.assertEqual(1, len(errors))

    def test_concept_names_follow_owning_namespace_style(self):
        valid_commit = MockCommit(
            files=[
                'Code/Framework/AzCore/AzCore/std/concepts/example.h',
                'Gems/Example/Code/Example.h',
            ],
            file_diffs={
                'Code/Framework/AzCore/AzCore/std/concepts/example.h': '+concept lower_snake_case = true;\n',
                'Gems/Example/Code/Example.h': '+concept PascalCase = true;\n',
            })
        errors = []
        self.assertTrue(Cpp20ConstraintValidator().run(valid_commit, errors))

        invalid_commit = MockCommit(
            files=[
                'Code/Framework/AzCore/AzCore/std/concepts/example.h',
                'Gems/Example/Code/Example.h',
            ],
            file_diffs={
                'Code/Framework/AzCore/AzCore/std/concepts/example.h': '+concept PascalCase = true;\n',
                'Gems/Example/Code/Example.h': '+concept lower_snake_case = true;\n',
            })
        errors = []
        self.assertFalse(Cpp20ConstraintValidator().run(invalid_commit, errors))
        self.assertEqual(2, len(errors))


if __name__ == '__main__':
    unittest.main()
