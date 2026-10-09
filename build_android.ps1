param(
    [string]$BuildDir = "build_android",
    [string]$NdkRoot = "",
    [string]$AndroidAbi = "arm64-v8a",
    [string]$AndroidPlatform = "android-21",
    [int]$Jobs = 18,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$source_dir = $PSScriptRoot
if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $build_dir = $BuildDir
} else {
    $build_dir = Join-Path $source_dir $BuildDir
}

if ([string]::IsNullOrWhiteSpace($NdkRoot)) {
    if (-not [string]::IsNullOrWhiteSpace($env:ANDROID_NDK_ROOT)) {
        $NdkRoot = $env:ANDROID_NDK_ROOT
    } elseif (-not [string]::IsNullOrWhiteSpace($env:ANDROID_NDK_HOME)) {
        $NdkRoot = $env:ANDROID_NDK_HOME
    } else {
        $NdkRoot = "C:\noinstall\android-ndk-r30"
    }
}

$toolchain_file = Join-Path $NdkRoot "build\cmake\android.toolchain.cmake"
if (-not (Test-Path -LiteralPath $toolchain_file -PathType Leaf)) {
    throw "Android NDK toolchain was not found: $toolchain_file. Pass -NdkRoot or set ANDROID_NDK_ROOT."
}

if ($Clean -and (Test-Path -LiteralPath $build_dir)) {
    Remove-Item -LiteralPath $build_dir -Recurse -Force
}

# Android NDK builds use Clang from the NDK. Ninja is the recommended CMake
# generator for Android on Windows and does not require a Visual Studio toolchain.
cmake -G "Ninja" `
    -DCMAKE_TOOLCHAIN_FILE="$toolchain_file" `
    -DANDROID_ABI="$AndroidAbi" `
    -DANDROID_PLATFORM="$AndroidPlatform" `
    -DCMAKE_BUILD_TYPE=Release `
    -DBEDROCK_LEVEL_BUILD_APPS=ON `
    -DBEDROCK_LEVEL_BUILD_TESTS=OFF `
    -B "$build_dir" `
    "$source_dir"

cmake --build "$build_dir" --parallel $Jobs
