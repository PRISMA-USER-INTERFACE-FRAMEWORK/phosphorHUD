param([string]$CommonLib = $env:COMMONLIBF4_PATH, [switch]$Package, [switch]$Clean)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $CommonLib) { $CommonLib = Join-Path (Split-Path -Parent $projectRoot) 'lib\CommonLibF4-NG' }
if (-not (Test-Path -LiteralPath (Join-Path $CommonLib 'xmake.lua'))) { throw 'CommonLibF4-NG checkout not found. Supply -CommonLib.' }
$env:COMMONLIBF4_PATH = $CommonLib
$log = Join-Path $projectRoot '.build\xmake.log'
New-Item -ItemType Directory -Force -Path (Split-Path $log) | Out-Null
"== build $(Get-Date -Format s)" | Set-Content -LiteralPath $log -Encoding UTF8
function Run([string]$what, [string[]]$xargs) {
    "`n== xmake $($xargs -join ' ')" | Add-Content -LiteralPath $log -Encoding UTF8
    $ErrorActionPreference = 'Continue'
    & xmake @xargs 2>&1 | ForEach-Object { $l = "$_" -replace "\x1b\[[0-9;]*m", ''; Add-Content -LiteralPath $log -Value $l -Encoding UTF8; $l } | Out-Host
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -ne 0) { throw "$what failed (exit $code)" }
}
Push-Location $projectRoot
try {
    $cfg = @('f', '-m', 'release', '-y', '--vs=2022')
    $flag = Join-Path $projectRoot '.build\clean'
    if ((Test-Path $flag) -and ((Get-Content $flag -Raw).Trim() -eq 'yes')) { $Clean = $true; Set-Content $flag 'no' }
    if ($Clean) { $cfg += '-c' }
    Run 'xmake configure' $cfg
    if ($env:PHOSPHOR_VERBOSE -or ((Test-Path (Join-Path $projectRoot '.build\verbose')) -and ((Get-Content (Join-Path $projectRoot '.build\verbose') -Raw).Trim() -eq 'yes'))) { Run 'core (verbose)' @('build', '-v', 'phosphor-core') }
    Run 'DLL build' @('build', 'PhosphorHUD')
    Run 'Test build' @('build', 'phosphor-tests')
    & .\build\tests\phosphor-tests.exe 2>&1 | ForEach-Object { Add-Content -LiteralPath $log -Value "$_" -Encoding UTF8; "$_" } | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
    if ($Package) { & "$PSScriptRoot\package.ps1" }
} finally { Pop-Location }
