#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import argparse
import pathlib
import subprocess
import tempfile


SHADER = b"""
ShaderResourceGroupSemantic SceneSemantic { FrequencyId = 0; };
ShaderResourceGroup SceneSrg : SceneSemantic
{
    float4 m_color;
};

[numthreads(1, 1, 1)]
void MainCS(uint3 dispatchId : SV_DispatchThreadID)
{
}
"""


def run(command, source):
    return subprocess.run(command, input=source, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)


def assert_process_matches(cli_result, api_result, description):
    assert cli_result.returncode == api_result.returncode, (
        f"{description}: exit codes differ: CLI={cli_result.returncode}, API={api_result.returncode}"
    )
    assert cli_result.stdout == api_result.stdout, f"{description}: stdout differs"
    assert cli_result.stderr == api_result.stderr, f"{description}: stderr differs"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--api-driver", required=True)
    args = parser.parse_args()

    compiler = pathlib.Path(args.compiler)
    api_driver = pathlib.Path(args.api_driver)

    # The ordinary stdin path is a direct API/CLI output comparison.
    assert_process_matches(
        run([compiler, "-"], SHADER),
        run([api_driver, "shader"], SHADER),
        "stdin shader emission",
    )

    # Developer modes must remain thin projections of the same API artifacts.
    for cli_flag, api_mode in (("--ast", "ast"), ("--dumpsym", "symbols")):
        assert_process_matches(
            run([compiler, cli_flag, "-"], SHADER),
            run([api_driver, api_mode], SHADER),
            cli_flag,
        )

    # Validation failures must preserve diagnostics and process exit behavior.
    invalid_source = b"void Broken("
    for cli_flag, api_mode in (("--syntax", "syntax"), ("--semantic", "semantic")):
        cli_result = run([compiler, cli_flag, "-"], invalid_source)
        api_result = run([api_driver, api_mode], invalid_source)
        assert_process_matches(cli_result, api_result, cli_flag)
        assert cli_result.returncode == 1, f"{cli_flag}: invalid input unexpectedly succeeded"
        assert b"stdin" in cli_result.stderr, f"{cli_flag}: diagnostic omitted the logical source path"

    # --full keeps shader output at -o and names every JSON artifact from that path.
    artifact_modes = {
        "ia": "ia",
        "om": "om",
        "srg": "srg",
        "options": "options",
        "bindingdep": "bindingdep",
    }
    with tempfile.TemporaryDirectory() as temporary_directory:
        output_path = pathlib.Path(temporary_directory) / "parity.result.hlsl"
        cli_result = run([compiler, "--full", "-o", output_path, "-"], SHADER)
        assert cli_result.returncode == 0, cli_result.stderr.decode(errors="replace")
        assert cli_result.stdout == b"", "--full with -o unexpectedly wrote shader output to stdout"

        shader_result = run([api_driver, "shader"], SHADER)
        assert output_path.read_bytes() == shader_result.stdout

        output_base = output_path.with_suffix("")
        for suffix, api_mode in artifact_modes.items():
            artifact_path = pathlib.Path(f"{output_base}.{suffix}.json")
            assert artifact_path.is_file(), f"--full did not create {artifact_path.name}"
            api_result = run([api_driver, api_mode], SHADER)
            assert api_result.returncode == 0, api_result.stderr.decode(errors="replace")
            assert artifact_path.read_bytes() == api_result.stdout, f"{suffix} artifact differs from API"


if __name__ == "__main__":
    main()
