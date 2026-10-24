param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$TestArgs
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
$TestRunner = Join-Path $BuildDir "self_test_runner.exe"
$Source = Join-Path (Join-Path $ProjectRoot "tests") "self_test.c"

if (-not (Test-Path $BuildDir -PathType Container)) {
    New-Item -ItemType Directory $BuildDir -Force | Out-Null
}

Write-Host "Compiling..." -ForegroundColor Cyan
gcc -std=c11 -Wall -Wextra -pedantic -Werror -o $TestRunner $Source
if (-not $?) {
    Write-Error "Compilation failed"
    exit 1
}

Write-Host "Running tests..." -ForegroundColor Cyan
if ($TestArgs) {
    & $TestRunner @TestArgs
} else {
    & $TestRunner
}

exit $LASTEXITCODE
