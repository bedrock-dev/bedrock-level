#Requires -Version 5.1
<#
.SYNOPSIS
    Build bedrock-level for wasm32 with Emscripten pthread support.
.EXAMPLE
    ./build_wasm.ps1 -EmsdkRoot C:/Users/xhy/dev/emsdk-4.0.7
.EXAMPLE
    ./build_wasm.ps1 -Apps
.EXAMPLE
    ./build_wasm.ps1 -WebDemo
#>
[CmdletBinding()]
param(
    [string]$BuildDir = 'build_wasm',
    [string]$EmsdkRoot = $(if ($env:EMSDK) { $env:EMSDK } else { Join-Path $env:USERPROFILE 'dev/emsdk-4.0.7' }),
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$EmscriptenVersion = '4.0.7',
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$BuildType = 'Release',
    [ValidateRange(0, 256)]
    [int]$Jobs = 0,
    [switch]$Apps,
    [switch]$Benchmarks,
    [switch]$WebDemo,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$sourceRoot = $PSScriptRoot
$buildRoot = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $sourceRoot $BuildDir }
$buildRoot = [IO.Path]::GetFullPath($buildRoot).TrimEnd('\', '/')
$emcmake = Join-Path $EmsdkRoot 'upstream/emscripten/emcmake.bat'
$sdkConfig = Join-Path $EmsdkRoot '.emscripten'
if ($Jobs -eq 0) { $Jobs = [Environment]::ProcessorCount }

foreach ($required in @($emcmake, $sdkConfig)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Missing Emscripten SDK file: $required. Pass -EmsdkRoot pointing to an activated SDK."
    }
}
foreach ($tool in @('cmake', 'ninja')) { [void](Get-Command $tool -ErrorAction Stop) }

foreach ($name in @('libleveldb-wasm32.a', 'libz-wasm32.a', 'libdeflate-wasm32.a')) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot "libs/$name") -PathType Leaf)) {
        throw "Missing libs/$name. Copy the prebuilt wasm32 dependencies into libs; see libs/README.md."
    }
}

if ($Clean -and (Test-Path -LiteralPath $buildRoot)) {
    # Only remove a resolved build directory inside this checkout.
    if (-not $buildRoot.StartsWith($sourceRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($buildRoot) -notmatch '^build[-_]') {
        throw '-Clean requires a build-* or build_* directory inside this repository.'
    }
    Remove-Item -LiteralPath $buildRoot -Recurse -Force
}

$previousConfig = $env:EM_CONFIG
try {
    $env:EM_CONFIG = $sdkConfig
    $versionOutput = & (Join-Path $EmsdkRoot 'upstream/emscripten/emcc.bat') --version
    if ($LASTEXITCODE -ne 0) { throw "Cannot query the Emscripten version ($LASTEXITCODE)." }
    $actualVersion = [regex]::Match(($versionOutput -join "`n"), 'emcc.*?\b(\d+\.\d+\.\d+)\b').Groups[1].Value
    if ($actualVersion -ne $EmscriptenVersion) {
        throw "Expected Emscripten $EmscriptenVersion, found '$actualVersion'. Pass a matching -EmsdkRoot or explicitly set -EmscriptenVersion for another Qt release."
    }
    $cacheFile = Join-Path $buildRoot 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cacheFile) {
        $toolchainEntry = Select-String -LiteralPath $cacheFile -Pattern '^CMAKE_TOOLCHAIN_FILE:[^=]+=(.+)$' | Select-Object -First 1
        $expectedToolchain = [IO.Path]::GetFullPath((Join-Path $EmsdkRoot 'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake'))
        if (-not $toolchainEntry -or -not [string]::Equals(
            [IO.Path]::GetFullPath($toolchainEntry.Matches[0].Groups[1].Value), $expectedToolchain, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Cached toolchain in $buildRoot differs from -EmsdkRoot. Rerun with -Clean."
        }
    }
    $versionFile = Join-Path $buildRoot 'wasm32-emscripten-version.txt'
    if ((Test-Path -LiteralPath $versionFile) -and (Get-Content -LiteralPath $versionFile -Raw).Trim() -ne $actualVersion) {
        throw 'Existing build uses another Emscripten version. Rerun with -Clean.'
    }
    & $emcmake cmake -S $sourceRoot -B $buildRoot -G Ninja `
        "-DCMAKE_BUILD_TYPE=$BuildType" `
        "-DBEDROCK_LEVEL_BUILD_APPS=$(if ($Apps) { 'ON' } else { 'OFF' })" `
        '-DBEDROCK_LEVEL_BUILD_TESTS=OFF' `
        "-DBEDROCK_LEVEL_BUILD_WEB_DEMO=$(if ($WebDemo) { 'ON' } else { 'OFF' })" `
        "-DBEDROCK_LEVEL_BUILD_BENCHMARKS=$(if ($Benchmarks) { 'ON' } else { 'OFF' })"
    if ($LASTEXITCODE -ne 0) { throw "Wasm configure failed ($LASTEXITCODE)." }
    cmake --build $buildRoot --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "Wasm build failed ($LASTEXITCODE)." }
    [IO.File]::WriteAllText($versionFile, "$actualVersion`n", [Text.UTF8Encoding]::new($false))

    $archive = Join-Path $buildRoot 'libbedrock-level-wasm32.a'
    Get-Item -LiteralPath $archive | Select-Object FullName, Length
    Write-Host 'This archive requires the three libs/*-wasm32.a dependencies. Use the bedrock-level CMake target to inherit link settings.'
    if ($WebDemo) {
        Write-Host "NBT viewer: $(Join-Path $buildRoot 'webdemo/index.html')"
        Write-Host "Serve with: python webdemo/serve.py --directory `"$(Join-Path $buildRoot 'webdemo')`""
    }
}
finally {
    $env:EM_CONFIG = $previousConfig
}
