[CmdletBinding()]
param([string]$BuildDirectory = "build/release")

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root $BuildDirectory

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -property installationPath
        $bundled = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $bundled) { $cmake = Get-Item $bundled }
    }
}
if (-not $cmake) { throw "CMake 3.24+ was not found in PATH or Visual Studio." }
$cmakePath = if ($cmake.Source) { $cmake.Source } else { $cmake.FullName }

& $cmakePath -S $root -B $build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with code $LASTEXITCODE." }
& $cmakePath --build $build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Release build failed with code $LASTEXITCODE." }

$exe = Join-Path $build "bin/Release/Peek.exe"
if (-not (Test-Path $exe)) {
    $exe = Join-Path $build "bin/Peek.exe"
}
if (-not (Test-Path $exe)) { throw "Release executable was not produced." }

$item = Get-Item $exe
Write-Host "Built: $($item.FullName)"
Write-Host "Size:  $($item.Length) bytes"
