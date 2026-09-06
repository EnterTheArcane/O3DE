"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

compiler = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="azsl-compiler-suite-") as temporary:
    tests = Path(temporary) / "Tests"
    shutil.copytree(Path(__file__).parent, tests, ignore=shutil.ignore_patterns("__pycache__"))
    result = subprocess.run(
        [sys.executable, "testapp.py", "--silent", "--compiler", compiler,
         "--path", "Syntax", "Semantic", "Advanced", "Samples"], cwd=tests, check=False)
    sys.exit(result.returncode)
