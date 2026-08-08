#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from commit_validation.crycommon_usage import line_uses_retired_source
from commit_validation.validators.crycommon_usage_validator import CryCommonUsageValidator


class FakeCommit:
    def __init__(self, file_name, diff):
        self._file_name = file_name
        self._diff = diff

    def get_files(self):
        return [self._file_name]

    def get_file_diff(self, _file_name):
        return self._diff


def test_retired_source_patterns_cover_helpers_headers_and_math_macros():
    assert line_uses_retired_source('#include <CryCommon/StlUtils.h>')
    assert line_uses_retired_source('#include "CryCommon/Cry_Math.h"')
    assert line_uses_retired_source('#include "Cry_Math.h"')
    assert line_uses_retired_source('stl::find_and_erase(values, value);')
    assert line_uses_retired_source('MiniQueue<int, 8> values;')
    assert line_uses_retired_source('float radians = DEG2RAD(degrees);')
    assert line_uses_retired_source('Vec3 direction;')
    assert line_uses_retired_source('Matrix34 transform;')
    assert line_uses_retired_source('Quat rotation;')
    assert line_uses_retired_source('ColorF tint;')
    assert line_uses_retired_source('const float angle = gf_PI;')
    assert line_uses_retired_source('sincos_tpl(angle, &sine, &cosine);')
    assert line_uses_retired_source('::Vec3 globalDirection;')
    assert line_uses_retired_source('::Matrix34 globalTransform;')
    assert line_uses_retired_source('::Plane clippingPlane;')
    assert line_uses_retired_source('AngleAxis rotation;')
    assert line_uses_retired_source('Diag33 scale;')
    assert line_uses_retired_source('::Limit(value, minimum, maximum);')
    assert line_uses_retired_source('::Lerp(start, end, t);')
    assert line_uses_retired_source('::sqr(length);')
    assert line_uses_retired_source('#define LEGACY_VECTOR Vec3')
    assert line_uses_retired_source('*out = Vec3(1.0f, 2.0f, 3.0f);')
    assert not line_uses_retired_source('AZStd::find(values.begin(), values.end(), value);')
    assert not line_uses_retired_source('AZ::Vector3 direction;')
    assert not line_uses_retired_source('AnimParamType::ColorB;')


def test_validator_rejects_added_source_use_and_dependency():
    validator = CryCommonUsageValidator()
    errors = []
    commit = FakeCommit(
        'Gems/Example/Code/CMakeLists.txt',
        '+    Legacy::CryCommon\n+#include <CryCommon/StlUtils.h>\n',
    )
    assert not validator.run(commit, errors)
    assert len(errors) == 2


def test_validator_ignores_removed_and_context_lines():
    validator = CryCommonUsageValidator()
    errors = []
    commit = FakeCommit(
        'Gems/Example/Code/Source/Example.cpp',
        '-stl::find(values, value);\n stl::find(values, value);\n+AZStd::find(values.begin(), values.end(), value);\n',
    )
    assert validator.run(commit, errors)
    assert not errors


def test_validator_defers_exact_compatibility_allowlist_to_full_tree_caps():
    validator = CryCommonUsageValidator()
    errors = []
    commit = FakeCommit(
        'Gems/LyShine/Code/Include/LyShine/UiBase.h',
        '+struct Vec4;\n+AZ_TYPE_INFO_SPECIALIZE(Vec4, "legacy-id");\n',
    )
    assert validator.run(commit, errors)
    assert not errors
