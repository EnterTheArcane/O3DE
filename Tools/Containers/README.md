# O3DE Windows x64 builds on Linux

The builder uses Linux x86-64 Docker, CMake, and Ninja. It builds the Windows
Profile configuration with `AutomatedTesting` enabled.

| Compiler | Windows toolchain |
| --- | --- |
| `clang-cl` | LLVM 21 and the Microsoft 14.44 headers and libraries from xwin |
| `msvc` | Microsoft 14.44 compiler and linker through Wine 10 |

Both use Windows SDK 10.0.26100. The Microsoft payloads come from the same
Visual Studio 2022 manifest. The xwin package list and compiler file check are
available under `/opt/microsoft` in the Clang and combined images.

## Build the image

From the engine root, review the [Microsoft license](https://go.microsoft.com/fwlink/?LinkId=2086102)
before passing the acceptance argument. Keep the resulting image and its build
cache local because they contain Microsoft Build Tools components.

```sh
docker build --platform linux/amd64 --build-arg ACCEPT_MICROSOFT_LICENSE=yes \
    -t o3de-windows-builder:local -f Tools/Containers/Builder/Dockerfile Tools/Containers
```

Use `--target clang-builder` or `--target msvc-builder` to provision only one
toolchain. The default image includes both.

## Stage the checkout and build

Use a Linux-native filesystem for the checkout and build trees. The following
commands copy the checkout, including unignored working-tree additions, into
Docker's Linux storage. Make sure Git LFS payloads are present in the checkout
before copying. Use a new volume when comparing a different revision or checkout.
On a Linux host, an existing ext4 checkout can also be mounted at `/work/source`.

```sh
docker run --rm --entrypoint bash \
    --mount "type=bind,src=$PWD,dst=/input,readonly" \
    --mount type=volume,src=o3de-cross-work,dst=/work \
    o3de-windows-builder:local -euo pipefail -c '
        mkdir -p /work/source
        git -c safe.directory=/input -C /input ls-files -z --cached --others --exclude-standard |
            tar -C /input --null -T - -cf - | tar -C /work/source -xf -
        git -c safe.directory=/input -C /input rev-parse HEAD > /work/source-revision
    '
```

```sh
run_windows() {
    docker run --rm \
        --mount type=volume,src=o3de-cross-work,dst=/work \
        --mount type=volume,src=o3de-cross-python,dst=/root/.o3de \
        -e O3DE_COMPILE_JOBS=16 -e O3DE_LINK_JOBS=2 \
        o3de-windows-builder:local "$@"
}

(
    set -e
    for compiler in clang-cl msvc; do
        run_windows "$compiler" probe
        run_windows "$compiler" configure
        run_windows "$compiler" build --target AzCore.Tests AzQtComponents
        run_windows "$compiler" build --target all
        run_windows "$compiler" build --target all
    done
)
```

The probe compiles a Windows resource, links an x64 executable with debug
symbols, prints its PE headers and imports, and checks the embedded resource
under Wine. The engine build keeps unity compilation enabled and prints its
wall time, disk usage, and container peak memory where available. The last
build should report that Ninja has no work to do. The two compilers have
independent build trees and share the O3DE package cache.

The MSVC route keeps a Wine server alive for each build, uses response files
for large link commands, and writes compiler diagnostics after the header
dependency records so Ninja can track them reliably. Reuse the same image for
incremental builds. Rebuilding the image can update toolchain header timestamps
and trigger recompilation.

This qualifies compilation and linking. Starting the Windows Editor also
requires a Windows Python environment with PySide6 registered.

The `configure` and `build` actions accept additional CMake arguments. To use
cmake-driver, mount its Linux launcher and a compatible .NET runtime, then set
`CMAKE_DRIVER` to the launcher path. Its logs persist under `/work/logs`.

## Run Windows tests under Wine

The `wine-test-runner` image adds Wine 11.22, Xvfb, and Mesa software graphics.
Use it with a build tree configured from the current sources. The `test` action
uses its own Wine prefix, installs the Microsoft runtime from the toolchain,
and starts executables through an engine-relative path so test fixtures can
find `engine.json` and the engine assets.

```sh
docker build --platform linux/amd64 --build-arg ACCEPT_MICROSOFT_LICENSE=yes \
    --target wine-test-runner -t o3de-windows-tests:local \
    -f Tools/Containers/Builder/Dockerfile Tools/Containers

docker run --init --rm --network none --shm-size 1g \
    --mount type=volume,src=o3de-cross-work,dst=/work \
    --mount type=volume,src=o3de-cross-python,dst=/root/.o3de \
    --entrypoint xvfb-run o3de-windows-tests:local -a \
    bash /opt/o3de/build.sh clang-cl test -j 8 \
    -L '^FRAMEWORK_(googletest|googlebenchmark)$' \
    --output-junit /work/clang-cl-cpp-tests.xml
```

For Windows pytest, provision a Windows Python environment with the pinned
`python/requirements.txt` and the editable test packages registered by the
engine and Gems. For embedded Python, place the environment and PySide6
registration in the test prefix under `%USERPROFILE%/.o3de/Python/venv/<engine-id>`.
The test prefix lives at `/work/wine/ctest/<compiler>`. Configure with
`-DLY_PYTEST_PYTHON_COMMAND='bash;/opt/o3de/wine-pytest.sh'` and set
`O3DE_WINE_PYTHON` to the path of that environment's `python.exe` inside the
container. Use `-j 1 -L '^FRAMEWORK_pytest$' -LE '^SUITE_awsi$'` for the local
pytest run. Editor and Asset Processor fixtures share files and processes.
Prepare the project asset cache with `AssetProcessorBatch.exe` under Wine
before running the Editor suites.
`O3DE_WINE_TEST_TIMEOUT` sets an additional per-entry deadline in seconds.
Software graphics can exercise startup and headless tests. GPU qualification
and AWS integration require their own environments.
