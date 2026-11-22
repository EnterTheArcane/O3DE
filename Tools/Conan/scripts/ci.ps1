param(
    [Parameter(Mandatory)][string]$Profile,
    [Parameter(Mandatory)][string]$Registry,
    [string]$BuildProfile,
    [string]$PublishedCatalog,
    [string]$CacheArchive
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
if (-not $BuildProfile) {
    $BuildProfile = python "$PSScriptRoot/build.py" build-profile
}
$buildOptions = @('--build-profile', $BuildProfile)
if ($CacheArchive) {
    $buildOptions += @('--cache-archive', $CacheArchive)
}

function Invoke-ConanBuild([string]$Command) {
    python -m pip install -r "$PSScriptRoot/../requirements.txt"
    python "$PSScriptRoot/build.py" $Command $Profile --output build/imaging @buildOptions
}

$android = $Profile -in @('android-a64', 'android-x64')
$wasm = $Profile -in @('emscripten-w32', 'emscripten-w64')
if ($android -or $wasm -or $Profile -in @('ios-a64', 'ios-simulator-a64')) {
    if (-not $PublishedCatalog -and $IsLinux -and ($android -or $Profile -eq 'emscripten-w32')) {
        bash "$PSScriptRoot/install_linux_sdk.sh" native
    }
    if ($PublishedCatalog) {
        $catalog = $PublishedCatalog
        if ($android) {
            Invoke-ConanBuild android-ndk
        }
    } else {
        Invoke-ConanBuild build
        python "$PSScriptRoot/publish.py" prepare $Profile --output build/imaging --registry $Registry --seed-cache build/verify-cache/oci
        $catalog = "build/imaging/$Profile.json"
    }
    $toolOptions = @()
    if ($android) {
        $ndk = bash -c 'source build/imaging/release/conanbuild.sh >/dev/null && printf "%s\n" "${ANDROID_NDK_ROOT:?}"'
        $toolOptions = @('--ndk', $ndk)
    }
    $verifyArgs = @("$PSScriptRoot/verify.py", '--profile', $Profile) + $toolOptions + @('--catalog', $catalog, '--cache', 'build/verify-cache', '--output', 'build/imaging-check')
    if ($wasm -and -not $PublishedCatalog) {
        bash -c 'source "$1" && shift && exec "$@"' emsdk build/imaging/release/conanbuild.sh python @verifyArgs
    } else {
        python @verifyArgs
    }
    if ($PublishedCatalog -and $android) {
        $abi = if ($Profile -eq 'android-a64') { 'arm64-v8a' } else { 'x86_64' }
        $engineOptions = @(
            "-DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/Platform/Android/Toolchain_android.cmake"
            "-DLY_NDK_DIR=$ndk"
            "-DANDROID_ABI=$abi"
            "-DO3DE_PREBUILT_OCI_CATALOG=$((Resolve-Path $catalog).Path)"
            "-DLY_PACKAGE_DOWNLOAD_CACHE_LOCATION=$((Resolve-Path build/verify-cache).Path)"
            '-DCMAKE_CONFIGURATION_TYPES=Profile'
            '-DLY_PROJECTS=AutomatedTesting'
        )
        cmake -S . -B build/imaging-engine -G 'Ninja Multi-Config' @engineOptions
        cmake --build build/imaging-engine --config Profile --target AutomatedTesting.GameLauncher --parallel
    }
    return
}

$cross = $BuildProfile -ne $Profile
$verifyOptions = @()
if ($cross) {
    $verifyOptions += '--cross'
}

if ($IsWindows) {
    $vs = & "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -property installationPath
    Import-Module "$vs/Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
    $arch = if ($Profile -eq 'windows-a64') { 'arm64' } else { 'amd64' }
    $hostArch = if ($BuildProfile -eq 'windows-a64') { 'arm64' } else { 'amd64' }
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=$arch -host_arch=$hostArch"
} elseif ($IsLinux) {
    $mode = if ($cross) { 'cross' } else { 'native' }
    bash "$PSScriptRoot/install_linux_sdk.sh" $mode
} elseif ($IsMacOS -and -not $PublishedCatalog -and -not (Get-Command 7z,7zz -ErrorAction SilentlyContinue)) {
    brew install sevenzip
}

if ($PublishedCatalog) {
    python "$PSScriptRoot/verify.py" --profile $Profile --prebuilt --catalog $PublishedCatalog --cache build/verify-cache --output build/imaging-check @verifyOptions
    if (-not $cross -and $Profile -ne 'windows-a64') {
        if ($IsLinux) {
            sudo apt-get update
            sudo apt-get install -y libevdev-dev libxcb-cursor0
            $env:CC = 'clang'
            $env:CXX = 'clang++'
        }
        git lfs pull --include '*.ico,*.bmp'
        $catalogPath = (Resolve-Path $PublishedCatalog).Path
        $cachePath = (Resolve-Path build/verify-cache).Path
        $engineOptions = @(
            "-DO3DE_PREBUILT_OCI_CATALOG=$catalogPath"
            "-DLY_PACKAGE_DOWNLOAD_CACHE_LOCATION=$cachePath"
            '-DCMAKE_CONFIGURATION_TYPES=Profile'
            '-DLY_PROJECTS=AutomatedTesting'
        )
        cmake -S . -B build/imaging-engine -G 'Ninja Multi-Config' @engineOptions
        $engineTargets = @('GradientSignal.Editor.Tests', 'AzNetworking.Tests', 'ImageProcessingAtom.Editor', 'QtForPython.Editor', 'AzTestRunner')
        cmake --build build/imaging-engine --config Profile --target @engineTargets --parallel
        ctest --test-dir build/imaging-engine -C Profile -R 'GradientSignal.Editor.Tests|AzNetworking.Tests' --no-tests=error --output-on-failure
    }
} else {
    Invoke-ConanBuild build
    python "$PSScriptRoot/test_packaging.py"
    python "$PSScriptRoot/publish.py" prepare $Profile --output build/imaging --registry $Registry --seed-cache build/verify-cache/oci
    python "$PSScriptRoot/verify.py" --profile $Profile --prebuilt --catalog "build/imaging/$Profile.json" --cache build/verify-cache --output build/imaging-check @verifyOptions
}
