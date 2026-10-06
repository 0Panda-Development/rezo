param(
    [string]$Version = (Get-Content (Join-Path $PSScriptRoot "version.txt")).Trim(),
    [string]$Token = ""
)

$ErrorActionPreference = "Stop"

if (-not $Token) {
    $tokenFile = Join-Path $PSScriptRoot "token.txt"
    if (Test-Path $tokenFile) { $Token = (Get-Content $tokenFile).Trim() }
}
if (-not $Token) {
    Write-Host "No GITHUB token. Create one at github.com/settings/tokens (repo scope),"
    Write-Host "save it in tools\updater\token.txt, then re-run this script."
    exit 1
}

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$release = Join-Path $root "release"
$api = "https://api.github.com"
$repo = "Pandajupiter8599/Rezo"
$headers = @{ Authorization = "token $Token"; "User-Agent" = "RezoPublish" }
$assets = @("rezo.zip", "version.txt", "RezoUpdater.exe", "Rezo-$Version.msix", "Rezo-selfsign.cer", "install.cmd", "RezoSetup.exe")

# Existing release with this tag? Reuse it (delete stale assets first).
$releaseUrl = "$api/repos/$repo/releases/tags/v$Version"
try {
    $existing = Invoke-RestMethod -Uri $releaseUrl -Headers $headers -Method Get
    Write-Host "Release v$Version exists - updating assets"
    foreach ($a in $existing.assets) {
        Invoke-RestMethod -Uri $a.url -Headers $headers -Method Delete | Out-Null
        Write-Host "  removed $($a.name)"
    }
    $releaseId = $existing.id
} catch {
    $rel = Invoke-RestMethod -Uri "$api/repos/$repo/releases" -Headers $headers -Method Post `
        -Body (@{ tag_name = "v$Version"; name = "v$Version"; body = "Rezo $Version" } | ConvertTo-Json)
    $releaseId = $rel.id
    Write-Host "Created release v$Version"
}

$releaseInfo = Invoke-RestMethod -Uri "$api/repos/$repo/releases/$releaseId" -Headers $headers -Method Get
$curl = "C:\Windows\System32\curl.exe"
foreach ($name in $assets) {
    $file = Join-Path $release $name
    if (-not (Test-Path $file)) { Write-Host "SKIP $name (not found)"; continue }
    Write-Host "Uploading $name ($((Get-Item $file).Length) bytes)..."
    $upUrl = $releaseInfo.upload_url.Replace("{?name,label}", "?name=$name")
    & $curl -sS -L --retry 4 --retry-all-errors --retry-delay 3 -X POST `
        -H "Authorization: token $Token" -H "Content-Type: application/octet-stream" `
        --data-binary "@$file" $upUrl | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "upload failed: $name" }
    Write-Host "  OK"
}

Write-Host "Published v${Version}: https://github.com/$repo/releases/tag/v$Version"