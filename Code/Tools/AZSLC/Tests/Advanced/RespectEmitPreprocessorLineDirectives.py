#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import os
import re

import common
from common import Foreground, Background, Style

"""
This test suite validates that azslc respects the preprocessor
#line <number> <filepath>
directives when outputting errors.
The idea is that when reporting errors it should produce meaningful filenames
and line locations from the orignal file where the error is actually coming from.
"""


def validate_files_appear_in_line_directives(hlsl_content, file_list, silent):
    """
    @fileList List of files to search for in @hlslContent.
              It is treated as a stack and we expect the line matching
              to occur in the order as they appear in this list              
    """
    regexp = re.compile('#\s*line\s+\d+\s*"(.*)"$')
    hlsl_lines = hlsl_content.splitlines()
    found0 = False
    for hlslLine in hlsl_lines:
        m = regexp.match(hlslLine)
        if not m:
            continue
        f0 = m.group(1).endswith(file_list[0])  # check top of stack
        f1 = len(file_list) > 1 and m.group(1).endswith(file_list[1])  # or second position to allow progression in the list
        if f0 or f1:
            if found0 and f1: del file_list[0]  # forget about a file only after its potential repetition is finished
            if len(file_list) == 0:
                break
            found0 = f0
        else:
            print(
                f"{Foreground.RED}problem: was expecting to find {file_list[0]} or {file_list[1]} (but got {m.group(1).rsplit('/', 1)[-1]}){Foreground.RESET}")
    return len(file_list) <= 1


def test_sample_file_compilation_emits_preprocessor_line_directives(the_file, compiler_path, silent):
    if not silent:
        print(f"{Foreground.CYAN}{Style.BRIGHT}"
              f"test_sample_file_compilation_emits_preprocessor_line_directives: "
              f"Verifying sample file compiles and #line directives are emitted..."
              f"{Style.RESET_ALL}")
    stdout, ok = common.build_and_get(the_file, compiler_path, silent, [])
    stdout = stdout.decode('utf-8')
    ok = validate_files_appear_in_line_directives(stdout,
                                                  ["SrgSemantics.azsli", "Level0.azsli", "Level1.azsli", "Level2.azsli", "Main.azsl"],
                                                  silent)
    return ok


def create_tmp_file_with_syntax_error(the_file, good_search_line, bad_replace_line):
    """
    Takes a reference file and creates of temporary clone file with a known good line (@goodSearchLine)
    that gets replaced with a known bad line (@badReplaceLine)
    """
    dir_name, file_name = os.path.split(the_file)
    tmp_file_path = os.path.join(dir_name, f"{file_name}.tmp")
    if os.path.exists(tmp_file_path): os.remove(tmp_file_path)

    found_good_search_line = False
    tmp_file_content = []
    with open(the_file) as fp:
        for cnt, line in enumerate(fp):
            line = line.rstrip('\r\n')
            if line == good_search_line:
                tmp_file_content.append(f"{bad_replace_line}\n")
                found_good_search_line = True
            else:
                tmp_file_content.append(f"{line}\n")
    if not found_good_search_line:
        print(f"{Foreground.RED}fail: {good_search_line} not found in {file_name}{Foreground.RESET}")
        return None

    with open(tmp_file_path, 'w') as outFp:
        outFp.writelines(tmp_file_content)
    return tmp_file_path


def test_error_report_uses_preprocessor_line_directives(the_file, compiler_path, silent, good_search_line, bad_replace_line, search_filename, error_type):
    """
    In this test an error will be injected at a specfic line and expect the stderr
    output produced by azslc to mention that the failure comes from one of the included files
    instead of the input file.
    """
    if not silent:
        print(f"{Foreground.CYAN}{Style.BRIGHT}"
              f"test_error_report_uses_preprocessor_line_directives: "
              f"Verifying syntax error report..."
              f"{Style.RESET_ALL}")
    file_path_of_tmp_file = create_tmp_file_with_syntax_error(the_file, good_search_line, bad_replace_line)
    if not file_path_of_tmp_file:
        return False
    if not silent:
        print(f"{Foreground.CYAN}{Style.BRIGHT}"
              f"test_error_report_uses_preprocessor_line_directives: "
              f"Compiling and expecting errors..."
              f"{Style.RESET_ALL}")
    stderr, failed = common.build_and_get_error(file_path_of_tmp_file, compiler_path, silent, [])
    stderr = stderr.decode('utf-8')
    if not failed:
        print(f"{Foreground.RED}fail: expected non-buildable didn't report a build error.{Foreground.RESET}")
        return False
    if not silent:
        print(f"{Foreground.CYAN}{Style.BRIGHT}"
              f"test_error_report_uses_preprocessor_line_directives: "
              f"Good, good compiler error, now let's make sure the source file is mentioned..."
              f"{Style.RESET_ALL}")
    if not search_filename in stderr:
        print(f"{Foreground.RED}fail: didn't find {search_filename} in stderr{Foreground.RESET}")
        return False
    if not silent:
        print(f"{Foreground.CYAN}{Style.BRIGHT}"
              f"test_error_report_uses_preprocessor_line_directives: "
              f"Good, The search file was mentioned, now let's check the type of error..."
              f"{Style.RESET_ALL}")
    ok = error_type in stderr
    if not ok:
        print(f"{Foreground.RED}fail: err #{error_type} not in stderr{Foreground.RESET}")
    return ok


result = 0  # to define for subtests
result_failed = 0


def do_tests(compiler, silent):
    global result
    global result_failed

    # Working directory should have been set to this script's directory by the calling parent
    # You can get it once do_tests() is called, but not during initialization of the module,
    #  because at that time it will still be set to the working directory of the calling script
    work_dir = os.getcwd()

    if test_sample_file_compilation_emits_preprocessor_line_directives(os.path.join(work_dir, "RespectEmitLine/Main.azsl.mcpp"),
                                                                       compiler, silent):
        result += 1
    else:
        print(f"{Foreground.RED}fail: test_sample_file_compilation_emits_preprocessor_line_directives{Foreground.RESET}")
        result_failed += 1

    if not silent: print("\n")
    if test_error_report_uses_preprocessor_line_directives(os.path.join(work_dir, "RespectEmitLine/Main.azsl.mcpp"),
                                                           compiler, silent,
                                                           "ShaderResourceGroup SRG2 : Slot2", "ShaderResour ceGroup SRG2 : Slot2",
                                                           "Level2.azsli",
                                                           "syntax error"):
        result += 1
    else:
        print(f"{Foreground.RED}fail: test_error_report_uses_preprocessor_line_directives{Foreground.RESET}")
        result_failed += 1

    if not silent: print("\n")
    if test_error_report_uses_preprocessor_line_directives(os.path.join(work_dir, "RespectEmitLine/Main.azsl.mcpp"),
                                                           compiler, silent,
                                                           "ShaderResourceGroup SRG1 : Slot1", "ShaderResourceGroup SRG1 : SlotX",
                                                           "Level1.azsli",
                                                           "Semantic error"):
        result += 1
    else:
        result_failed += 1

    # if testSemanticErrorReportUsesPreprocessorLineDirectives(os.path.join(work_dir, "RespectEmitLine/Main.azsl.mcpp"),
    #                                                       compiler, silent): result += 1
    # else: result_failed += 1


if __name__ == "__main__":
    assert "please call from runner.py"
