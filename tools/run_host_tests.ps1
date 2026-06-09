# Build and run the portable-logic host tests with zig cc (self-contained C
# compiler via `pip install ziglang`). No ESP-IDF required.
#
#   python -m pip install ziglang
#   pwsh tools/run_host_tests.ps1
#
# A plain CMakeLists.txt is also provided for users with gcc/clang+cmake.
param([switch]$KeepGoing)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

$includes = @(
    "$root\components\mesh\include",
    "$root\components\mesh\nanopb",
    "$root\components\mesh\generated",
    "$root\components\core\include",
    "$root\components\net\include",
    "$root\components\ui\include",
    "$root\components\util\include",
    "$root\host_tests"
)

# Only PORTABLE sources (no ESP-IDF/LVGL deps) go into the host build.
$srcGlobs = @(
    "$root\components\mesh\*.c",
    "$root\components\mesh\nanopb\*.c",
    "$root\components\mesh\generated\*.c",
    "$root\components\core\*.c",
    "$root\components\net\portable\*.c",
    "$root\components\ui\portable\*.c",
    "$root\components\util\*.c",
    "$root\host_tests\*.c"
)

$src = $srcGlobs | ForEach-Object { Get-ChildItem $_ -ErrorAction SilentlyContinue } |
    Select-Object -ExpandProperty FullName |
    Where-Object { $_ -notmatch '_nvs\.c$' }   # device-only (NVS); not host-portable
$incArgs = $includes | ForEach-Object { "-I$_" }
$cflags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-sign-compare', '-lm')

New-Item -ItemType Directory -Force -Path "$root\build-host" | Out-Null
$exe = "$root\build-host\ht.exe"

Write-Host "Compiling $($src.Count) sources..." -ForegroundColor Cyan
& python -m ziglang cc @cflags @incArgs @src -o $exe
if ($LASTEXITCODE -ne 0) { Write-Error "compile failed ($LASTEXITCODE)"; exit 1 }

& $exe
$rc = $LASTEXITCODE
if ($rc -ne 0 -and -not $KeepGoing) { exit $rc }
exit $rc
