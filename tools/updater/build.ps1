$ErrorActionPreference = "Stop"

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$dist = Join-Path $root "dist\Rezo"
$release = Join-Path $root "release"
$stage = Join-Path $env:TEMP "rezo-stage"
$version = (Get-Content (Join-Path $PSScriptRoot "version.txt")).Trim()

New-Item -ItemType Directory -Path $release -Force | Out-Null

# Build the updater FIRST so the package can ship it (self-update).
$outExe = Join-Path $release "RezoUpdater.exe"
$srcCs = Join-Path $PSScriptRoot "updater.cs"
$ico = Join-Path $PSScriptRoot "rezo.ico"
$png = Join-Path $PSScriptRoot "logo-rezo.png"
& "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /optimize+ /target:winexe `
    /win32icon:$ico `
    /resource:$ico,logo.ico `
    /resource:$png,logo.png `
    /out:$outExe `
    /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:Microsoft.CSharp.dll `
    /r:System.Windows.Forms.dll /r:System.Drawing.dll `
    $srcCs
if ($LASTEXITCODE -ne 0) { throw "csc failed" }

# Code signing (optional, inactive until you add credentials).
# Get Azure Trusted Signing (~$9.99/mo, Microsoft's own) or any code-signing
# cert. Then drop the signing config in tools\updater\sign.ps1 (see below) and
# the release exe is auto-signed and passes Smart App Control for everyone.
$signScript = Join-Path $PSScriptRoot "sign.ps1"
if (Test-Path $signScript) {
    & $signScript -Exe $outExe
    if ($LASTEXITCODE -ne 0) { throw "signing failed" }
}

Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item $dist $stage -Recurse
Copy-Item (Join-Path $PSScriptRoot "version.txt") (Join-Path $stage "version.txt")
Copy-Item $outExe (Join-Path $stage "RezoUpdater.exe")
$urlFile = Join-Path $PSScriptRoot "rezo-update.url"
if (Test-Path $urlFile) { Copy-Item $urlFile (Join-Path $stage "rezo-update.url") }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath (Join-Path $release "rezo.zip") -Force

# The published version.txt drives every updater's update check - publish it
# alongside the zip or everyone sees a stale server version forever.
Copy-Item (Join-Path $PSScriptRoot "version.txt") (Join-Path $release "version.txt") -Force

"release ready:"
Get-ChildItem $release | Select-Object Name, Length