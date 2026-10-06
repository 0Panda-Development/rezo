param(
    [string]$Dist = (Join-Path (Split-Path $PSScriptRoot -Parent) "filters"),
    [string]$Lists = (Join-Path (Split-Path $PSScriptRoot -Parent) "filters\lists")
)
$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
New-Item $Dist -ItemType Directory -Force | Out-Null
New-Item $Lists -ItemType Directory -Force | Out-Null

$sources = [ordered]@{
    "stevenblack" = "https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts"
    "peterlowe"   = "https://pgl.yoyo.org/adservers/serverlist.php?hostformat=hosts&showintro=0&mimetype=plaintext"
    "anudeep"     = "https://raw.githubusercontent.com/anudeepND/blacklist/master/adservers.txt"
    "oisd"        = "https://big.oisd.nl/domains"
    "easylist"    = "https://easylist.to/easylist/easylist.txt"
    "easyprivacy" = "https://easylist.to/easylist/easyprivacy.txt"
}
foreach ($k in $sources.Keys) {
    $p = Join-Path $Lists "$k.txt"
    if (-not (Test-Path $p) -or ((Get-Date) - (Get-Item $p).LastWriteTime).TotalDays -gt 3) {
        try { Invoke-WebRequest -Uri $sources[$k] -OutFile $p -UseBasicParsing -TimeoutSec 120 } catch { Write-Host "skip $k" }
    }
}

$hostSet = New-Object System.Collections.Generic.HashSet[string]
$urlRules = New-Object System.Collections.Generic.HashSet[string]
$cosmetic = New-Object "System.Collections.Generic.HashSet[string]"

function Add-Host($h) {
    $h = $h.Trim().ToLower().TrimEnd('.')
    if ($h -ne "" -and $h -match '^[a-z0-9._-]+\.[a-z0-9-]{2,}$' -and $h -notmatch '^(\d+\.)+\d+$' -and $h -ne "localhost") { [void]$hostSet.Add($h) }
}

$badSel = ':-abp|:has-text|:xpath|:matches-|:contains\(|:upward|:remove|:style|:nth-ancestor|:min-text|:watch|:others|:-js|\+js\(|\^|\t|\{|\}'
function Add-Cosmetic($domains, $selector) {
    $sel = $selector.Trim()
    if ($sel -eq '' -or $sel -match $badSel) { return }
    if ($domains -eq '*' -or [string]::IsNullOrWhiteSpace($domains)) { [void]$cosmetic.Add("`t$sel"); return }
    foreach ($d in ($domains -split ',')) {
        $d = $d.Trim().ToLower()
        if ($d -eq '' -or $d.StartsWith('~') -or $d.Contains('*')) { continue }
        [void]$cosmetic.Add("$d`t$sel")
    }
}

foreach ($f in @("stevenblack", "peterlowe", "anudeep", "oisd")) {
    $path = Join-Path $Lists "$f.txt"
    if (-not (Test-Path $path)) { continue }
    foreach ($line in [System.IO.File]::ReadLines($path)) {
        $l = $line; $hash = $l.IndexOf('#'); if ($hash -ge 0) { $l = $l.Substring(0, $hash) }
        $t = $l.Trim()
        if ($t -eq '') { continue }
        if ($t -match '^(?:0\.0\.0\.0|127\.0\.0\.1)\s+(\S+)$') { Add-Host $Matches[1] }
        elseif ($t -notmatch '[\s/*]') { Add-Host $t }
    }
}

$skipOpts = 'domain=|badfilter|csp=|redirect|removeparam|replace=|rewrite|denyallow|~|generichide|elemhide|document|inline-script'
foreach ($f in @("easylist", "easyprivacy")) {
    $path = Join-Path $Lists "$f.txt"
    if (-not (Test-Path $path)) { continue }
    foreach ($raw in [System.IO.File]::ReadLines($path)) {
        $line = $raw.Trim()
        if ($line -eq '' -or $line[0] -eq '[' -or $line[0] -eq '!') { continue }
        if ($line.StartsWith('@@')) { continue }
        if ($line -match '^([^#]*)##(.+)$') { Add-Cosmetic $Matches[1] $Matches[2]; continue }
        if ($line.Contains('#@#') -or $line.Contains('#?#') -or $line.Contains('#$#') -or $line.Contains('#%#')) { continue }
        $opts = ''
        $rule = $line
        $di = $line.LastIndexOf('$')
        if ($di -ge 0) { $opts = $line.Substring($di + 1); $rule = $line.Substring(0, $di) }
        if ($opts -match $skipOpts) { continue }
        if ($rule -match '^\|\|([a-z0-9.\-]+)\^?$') { Add-Host $Matches[1]; continue }
        if ($rule -match '^\|\|([a-z0-9.\-]+/[^*^|]+)$') { [void]$urlRules.Add($Matches[1].ToLower()); continue }
        if ($rule -match '^\|\|([a-z0-9.\-]+)\^([^*^|]+)$') { [void]$urlRules.Add($Matches[1].ToLower() + '/' + $Matches[2].TrimStart('/').ToLower()); continue }
        if ($rule -match '^/[a-z0-9_\-./]{5,}$') { [void]$urlRules.Add($rule.ToLower()); continue }
    }
}

$hostSet | Sort-Object | ForEach-Object { "0.0.0.0 $_" } | Set-Content (Join-Path $Dist "blocklist.txt") -Encoding ASCII
$urlRules | Sort-Object | Set-Content (Join-Path $Dist "urlrules.txt") -Encoding ASCII
$cosmetic | Sort-Object | Set-Content (Join-Path $Dist "cosmetic.txt") -Encoding UTF8

"hosts: $($hostSet.Count) url: $($urlRules.Count) cosmetic: $($cosmetic.Count)"
