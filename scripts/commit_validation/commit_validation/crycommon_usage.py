#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import os
import re
from pathlib import Path
from typing import List, Tuple


RETIRED_HEADER_PATTERNS = (
    re.compile(r'#\s*include\s*[<"](?:CryCommon/)?(?:StlUtils|MiniQueue)\.h[>"]'),
    re.compile(
        r'#\s*include\s*[<"](?:CryCommon/)?(?:Cry_Math|Cry_Color|Cry_Vector[234]|Cry_Matrix(?:33|34|44)|'
        r'Cry_Quat|Cry_ValidNumber|MathConversion|LCGRandom)\.h[>"]'
    ),
)

RETIRED_CODE_PATTERNS = (
    re.compile(r'\bstl::'),
    re.compile(r'\bMiniQueue\s*<'),
    re.compile(
        r'(?<!\w::)\b(?:Vec[234](?:_tpl)?|Matrix(?:33|34|44)(?:_tpl)?|Quat(?:_tpl|T)?|Ang3(?:_tpl)?|'
        r'Plane_tpl|AngleAxis|Diag33|Color[BF])\b'
    ),
    re.compile(r'(?<!\w)::(?:Plane|Limit|Lerp|sqr)\b'),
    re.compile(r'\b(?:gf_PI|gf_PI2|gf_halfPI|g_PI|g_PI2)\b'),
    re.compile(
        r'\b(?:clamp_tpl|cry_random|cry_frand|cry_srand|int_round|pos_round|int_ceil|NumberValid|'
        r'sincos_tpl|fabs_tpl|floor_tpl|ceil_tpl|fmod_tpl|cos_tpl|sin_tpl|acos_tpl|asin_tpl|atan_tpl|'
        r'atan2_tpl|tan_tpl|exp_tpl|log_tpl|pow_tpl|sqrt_tpl|sqrt_fast_tpl|isqrt_tpl|isqrt_fast_tpl|'
        r'isqrt_safe_tpl|fsel|fres|isel|iselnz|fzero)\s*\('
    ),
    re.compile(r'\b(?:DEG2RAD|RAD2DEG|DEG2COS|COS2DEG|RAD2HCOS|HCOS2RAD|DEG2HCOS|DEG2HSIN|HCOS2DEG)\s*\('),
)

SOURCE_PATTERN_MAXIMUMS = {
    # Existing compatibility/transitional uses may decrease but cannot increase.
    'Code/Legacy/CrySystem/XML/xml.cpp': 1,
    'Code/Legacy/CrySystem/XML/XMLBinaryNode.cpp': 1,
    'Code/Editor/Lib/Tests/test_CryLegacyDeprecation.cpp': 14,
    # Exported legacy Vec4 UUID compatibility declaration; conversion is owned by MathReflect.
    'Gems/LyShine/Code/Include/LyShine/UiBase.h': 2,
    # These are unrelated local/enum names, not CryCommon declarations.
    'Gems/NvCloth/Code/Source/Utils/MeshAssetHelper.cpp': 7,
    'Gems/Maestro/Code/Source/Cinematics/Movie.cpp': 1,
}

# Existing dependencies are capped per target file. A target may reduce its count, but the full-tree
# check fails if a new file is added or any existing file grows. Lifecycle-owned entries are tracked
# here until their subsystem migrations can be reviewed separately.
CRYCOMMON_DEPENDENCY_MAXIMUMS = {
    'Code/Editor/CMakeLists.txt': 5,
    'Code/LauncherUnified/CMakeLists.txt': 5,
    'Code/Legacy/CrySystem/CMakeLists.txt': 2,
    'Code/Legacy/CrySystem/XML/CMakeLists.txt': 1,
    'Code/Tools/RemoteConsole/CMakeLists.txt': 1,
    'Gems/AssetValidation/Code/CMakeLists.txt': 1,
    'Gems/AtomLyIntegration/AtomBridge/Code/CMakeLists.txt': 1,
    'Gems/AtomLyIntegration/AtomFont/Code/CMakeLists.txt': 1,
    'Gems/AtomLyIntegration/CommonFeatures/Code/CMakeLists.txt': 1,
    'Gems/AudioSystem/Code/CMakeLists.txt': 3,
    'Gems/CertificateManager/Code/CMakeLists.txt': 1,
    'Gems/DebugDraw/Code/CMakeLists.txt': 2,
    'Gems/EMotionFX/Code/CMakeLists.txt': 2,
    'Gems/GameStateSamples/Code/CMakeLists.txt': 1,
    'Gems/Gestures/Code/CMakeLists.txt': 1,
    'Gems/ImGui/Code/CMakeLists.txt': 1,
    'Gems/LandscapeCanvas/Code/CMakeLists.txt': 1,
    'Gems/LmbrCentral/Code/CMakeLists.txt': 3,
    'Gems/LmbrCentral/Code/Tests/CMakeLists.txt': 2,
    'Gems/LyShine/Code/CMakeLists.txt': 8,
    'Gems/LyShineExamples/Code/CMakeLists.txt': 1,
    'Gems/Maestro/Code/CMakeLists.txt': 4,
    'Gems/MessagePopup/Code/CMakeLists.txt': 1,
    'Gems/Microphone/Code/CMakeLists.txt': 1,
    'Gems/PhysX/Debug/PhysX5/CMakeLists.txt': 2,
    'Gems/SceneLoggingExample/Code/CMakeLists.txt': 1,
    'Gems/ScriptCanvas/Code/CMakeLists.txt': 1,
    'Gems/ScriptedEntityTweener/Code/CMakeLists.txt': 1,
    'Gems/TextureAtlas/Code/CMakeLists.txt': 3,
    'Gems/TickBusOrderViewer/Code/CMakeLists.txt': 1,
    'Gems/Vegetation/Code/CMakeLists.txt': 4,
    'Gems/VirtualGamepad/Code/CMakeLists.txt': 1,
    'Gems/WhiteBox/Code/CMakeLists.txt': 2,
}

SOURCE_EXTENSIONS = {'.c', '.cc', '.cpp', '.cxx', '.h', '.hpp', '.hxx', '.inl', '.m', '.mm'}


def _strip_comments_and_literals(
    line: str,
    in_block_comment: bool = False,
    strip_literals: bool = True,
) -> Tuple[str, bool]:
    """Return C/C++ code with comments and string/character literals removed.

    The block-comment state is returned so full-file scans correctly distinguish a
    documentation line beginning with ``*`` from a dereference expression such as
    ``*out = Vec3(...)``.
    """
    code = []
    index = 0
    while index < len(line):
        if in_block_comment:
            comment_end = line.find('*/', index)
            if comment_end == -1:
                return ''.join(code), True
            in_block_comment = False
            index = comment_end + 2
            continue

        if line.startswith('//', index):
            break
        if line.startswith('/*', index):
            in_block_comment = True
            index += 2
            continue
        if strip_literals and line[index] in ('"', "'"):
            delimiter = line[index]
            index += 1
            while index < len(line):
                if line[index] == '\\':
                    index += 2
                    continue
                if line[index] == delimiter:
                    index += 1
                    break
                index += 1
            code.append(' ')
            continue
        code.append(line[index])
        index += 1
    return ''.join(code), in_block_comment


def line_uses_retired_source(line: str, relative_path: str = '') -> bool:
    comment_stripped_code, _ = _strip_comments_and_literals(line, strip_literals=False)
    if any(pattern.search(comment_stripped_code) for pattern in RETIRED_HEADER_PATTERNS):
        return True
    if re.match(r'^\s*#\s*include\b', comment_stripped_code):
        return False
    code, _ = _strip_comments_and_literals(comment_stripped_code)
    if relative_path.startswith('Code/Framework/AzCore/AzCore/Math/'):
        # AzCore's SIMD implementation has unrelated internal Vec2/Vec3/Vec4 types. Header and
        # uniquely named helper checks above still apply in this directory.
        code = re.sub(r'\bVec[234]\b', '', code)
    return any(pattern.search(code) for pattern in RETIRED_CODE_PATTERNS)


def scan_tree(repository_root: Path) -> List[str]:
    errors: List[str] = []
    dependency_counts = {}

    for source_root in ('Code', 'Gems'):
        root = repository_root / source_root
        for path in root.rglob('*'):
            if not path.is_file() or 'build' in path.parts or 'Cache' in path.parts:
                continue
            relative_path = path.relative_to(repository_root).as_posix()
            if relative_path.startswith('Code/Legacy/CryCommon/'):
                continue

            if path.name == 'CMakeLists.txt':
                count = path.read_text(encoding='utf8', errors='replace').count('Legacy::CryCommon')
                if count:
                    dependency_counts[relative_path] = count

            if path.suffix.lower() not in SOURCE_EXTENSIONS:
                continue
            retired_source_uses = []
            in_block_comment = False
            for line_number, line in enumerate(path.read_text(encoding='utf8', errors='replace').splitlines(), 1):
                code, in_block_comment = _strip_comments_and_literals(
                    line, in_block_comment, strip_literals=False)
                if line_uses_retired_source(code, relative_path):
                    retired_source_uses.append((line_number, line.strip()))

            maximum = SOURCE_PATTERN_MAXIMUMS.get(relative_path, 0)
            if len(retired_source_uses) > maximum:
                for line_number, line in retired_source_uses[maximum:]:
                    errors.append(f'{relative_path}:{line_number}: retired CryCommon source use: {line}')

    for path, count in dependency_counts.items():
        maximum = CRYCOMMON_DEPENDENCY_MAXIMUMS.get(path, 0)
        if count > maximum:
            errors.append(f'{path}: Legacy::CryCommon dependency count {count} exceeds allowlisted maximum {maximum}')

    return errors
