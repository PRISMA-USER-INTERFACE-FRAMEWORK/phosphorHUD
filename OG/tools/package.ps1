$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$modRoot = Join-Path $projectRoot 'mod\Phosphor HUD'
$pluginRoot = Join-Path $modRoot 'F4SE\Plugins'
$dll = Join-Path $projectRoot 'build\release\PhosphorHUD.dll'
if (-not (Test-Path -LiteralPath $dll)) { throw 'Build the DLL first with tools/build.ps1.' }
New-Item -ItemType Directory -Force -Path (Join-Path $pluginRoot 'PhosphorHUD') | Out-Null
Copy-Item -LiteralPath $dll -Destination $pluginRoot -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'build\release\PhosphorHUD.pdb') -Destination $pluginRoot -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'examples\messages.jsonl') -Destination (Join-Path $pluginRoot 'PhosphorHUD\replay.jsonl') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'schema\hud-messages.schema.json') -Destination (Join-Path $pluginRoot 'PhosphorHUD\hud-messages.schema.json') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\STAGE0_TEST.md') -Destination (Join-Path $modRoot 'PhosphorHUD-Stage0-README.md') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'third_party\licenses') -Destination $modRoot -Recurse -Force
$dist = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$version = ([regex]::Match((Get-Content (Join-Path $projectRoot 'xmake.lua') -Raw), 'set_version\("([^"]+)"\)')).Groups[1].Value
$archive = Join-Path $dist "PhosphorHUD-$version-personal.zip"
Compress-Archive -Path (Join-Path $modRoot '*') -DestinationPath $archive -Force
Get-Item -LiteralPath $archive | Select-Object FullName,Length
Get-FileHash -LiteralPath $archive -Algorithm SHA256
