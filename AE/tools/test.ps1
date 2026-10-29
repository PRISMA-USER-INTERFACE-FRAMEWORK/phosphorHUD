param([Parameter(Mandatory=$true)][string]$Python)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (Test-Path -LiteralPath (Join-Path $projectRoot '.tools\python')) {
    $env:PYTHONPATH = Join-Path $projectRoot '.tools\python'
}
if (Test-Path -LiteralPath (Join-Path $projectRoot '.tools\browsers')) {
    $env:PLAYWRIGHT_BROWSERS_PATH = Join-Path $projectRoot '.tools\browsers'
}
Push-Location $projectRoot
try {
    & .\build\tests\phosphor-tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
    & $Python -m unittest discover -s tests -v
    if ($LASTEXITCODE -ne 0) { throw 'Regression suite failed' }
    & $Python tools/replay.py
    if ($LASTEXITCODE -ne 0) { throw 'Contract replay failed' }
} finally { Pop-Location }
