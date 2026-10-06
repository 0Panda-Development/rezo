param(
    [string]$Version = (Get-Content (Join-Path $PSScriptRoot "..\tools\updater\version.txt")).Trim(),
    [string]$Publisher = "CN=Pandajupiter",
    [string]$Install = "true"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$dist = Join-Path $root "dist\Rezo"
$release = Join-Path $root "release"
$stage = Join-Path $env:TEMP "rezo-msix-stage"
$assets = Join-Path $stage "Assets"
$ver4 = if (($Version -split '\.').Count -ge 4) { $Version } else { $Version + ".0" }  # MSIX needs a 4-part version
$pkg = Join-Path $release "Rezo-$Version.msix"
$certPath = Join-Path $root "tools\updater\rezo-selfsign.pfx"
$makeappx = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\makeappx.exe"
$signtool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"

Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $assets -Force | Out-Null

# --- Generate app logos from the original Rezo icon (not the placeholder "R") ---
# Note: the ico's 256px frame has the design in the top-left corner only;
# the small frames are correct, so upscale from the 48px frame.
Add-Type -AssemblyName System.Drawing
$icoPath = Join-Path $root "tools\updater\rezo.ico"
$icon = New-Object System.Drawing.Icon $icoPath, 48, 48
$srcBmp = $icon.ToBitmap()
function New-Logo($path, $width, $height = $width) {
    $bmp = New-Object System.Drawing.Bitmap $width, $height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($srcBmp, 0, 0, $width, $height)
    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}
New-Logo (Join-Path $assets "StoreLogo.png") 50
New-Logo (Join-Path $assets "Square44x44Logo.png") 44
New-Logo (Join-Path $assets "Square44x44Logo.targetsize-44.png") 44
New-Logo (Join-Path $assets "Square71x71Logo.png") 71
New-Logo (Join-Path $assets "Square150x150Logo.png") 150
New-Logo (Join-Path $assets "Square310x310Logo.png") 310
New-Logo (Join-Path $assets "Wide310x150Logo.png") 310 150

# --- Manifest ---
$manifest = @"
<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
         xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
         xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities">
  <Identity Name="Pandajupiter.Rezo" Publisher="$Publisher" Version="$ver4" ProcessorArchitecture="x64"/>
  <Properties>
    <DisplayName>Rezo</DisplayName>
    <PublisherDisplayName>Pandajupiter</PublisherDisplayName>
    <Description>Rezo privacy browser</Description>
    <Logo>Assets\StoreLogo.png</Logo>
  </Properties>
  <Resources>
    <Resource Language="en-us"/>
  </Resources>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.17763.0" MaxVersionTested="10.0.26100.0"/>
  </Dependencies>
  <Capabilities>
    <rescap:Capability Name="runFullTrust"/>
  </Capabilities>
  <Applications>
    <Application Id="Rezo" Executable="rezo.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="Rezo" Description="Rezo privacy browser"
        Square150x150Logo="Assets\Square150x150Logo.png"
        Square44x44Logo="Assets\Square44x44Logo.png"
        BackgroundColor="#0B0F1A"/>
    </Application>
  </Applications>
</Package>
"@
Set-Content (Join-Path $stage "AppxManifest.xml") $manifest -Encoding UTF8

# --- Payload: everything the exe needs (skip tests + icons) ---
Get-ChildItem $dist | Where-Object { $_.Name -notmatch 'rezo_tests\.exe' } | Copy-Item -Destination $stage -Recurse -Force
Move-Item (Join-Path $stage "Assets") $assets -Force -ErrorAction SilentlyContinue

# --- Self-signed cert (local build/sideload; Store re-signs on submission) ---
$pfxPass = "rezo-local"
if (Test-Path $certPath) {
    # Reuse the existing cert so trust persists across releases
    $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 $certPath, $pfxPass
} else {
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $Publisher `
        -CertStoreLocation Cert:\CurrentUser\My -KeyExportPolicy Exportable
    Export-PfxCertificate -Cert $cert -FilePath $certPath -Password (ConvertTo-SecureString $pfxPass -AsPlainText -Force) | Out-Null
}
# Public cert for distribution - always matches the cert we sign with
Export-Certificate -Cert $cert -FilePath (Join-Path $release "Rezo-selfsign.cer") -Type CERT | Out-Null

# --- Pack ---
$mapping = Join-Path $stage "mapping.txt"
"[Files]" | Set-Content $mapping -Encoding ASCII
Get-ChildItem $stage -File -Recurse | ForEach-Object {
    $rel = $_.FullName.Substring($stage.Length + 1)
    "`"$($_.FullName)`" `"$rel`"" | Add-Content $mapping -Encoding ASCII
}
& $makeappx pack /f $mapping /p $pkg /o
if ($LASTEXITCODE -ne 0) { throw "makeappx failed" }

# --- Sign ---
& $signtool sign /f $certPath /p $pfxPass /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $pkg
if ($LASTEXITCODE -ne 0) { throw "signtool failed" }

# --- Trust our cert so the package verifies/installs on this machine ---
try {
    Import-PfxCertificate -FilePath $certPath -CertStoreLocation Cert:\LocalMachine\Root -Password (ConvertTo-SecureString $pfxPass -AsPlainText -Force) | Out-Null
} catch {
    Import-PfxCertificate -FilePath $certPath -CertStoreLocation Cert:\CurrentUser\Root -Password (ConvertTo-SecureString $pfxPass -AsPlainText -Force) | Out-Null
}

# --- Verify + install (sideload) ---
& $signtool verify /pa /v $pkg 2>&1 | Select-Object -Last 2
if ($Install -eq "true") {
    reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock" /t REG_DWORD /f /v AllowAllTrustedApps /d 1 | Out-Null
    Get-AppxPackage -Name "Pandajupiter.Rezo" -ErrorAction SilentlyContinue | Remove-AppxPackage
    Add-AppxPackage -Path $pkg -ForceApplicationShutdown
    if ($?) { Write-Host "Installed: $pkg" } else { Write-Host "Sideload install failed (see above)" }
}

Write-Host "MSIX package ready: $pkg"
Get-Item $pkg | Select-Object FullName, @{n='MB';e={[math]::Round($_.Length/1MB)}}