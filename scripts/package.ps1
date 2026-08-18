param([string]$Config = "Release")

$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root "build"
$out = Join-Path $root "dist\Rezo"

# Locate the built exe (CEF may use build\bin\Release or build\Release).
$exe = Get-ChildItem -Path (Join-Path $build "bin\$Config"), (Join-Path $build $Config) `
    -Filter "Rezo.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $exe) { throw "Rezo.exe not found in build output" }

Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
New-Item $out -ItemType Directory -Force | Out-Null

# Everything the exe needs: its whole output dir (CEF post-build copies
# libcef.dll, resources.pak, locales, etc. next to it).
Get-ChildItem $exe.Directory | Copy-Item -Destination $out -Recurse -Force

# Tor bundle + resources next to the exe.
Copy-Item (Join-Path $root "tor") $out -Recurse -Force
Copy-Item (Join-Path $root "src\resources\*") $out -Force

Write-Host "Packaged to $out"