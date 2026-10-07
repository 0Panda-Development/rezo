$lines = Get-Content E:\Rezo\tools\updater\updater.cs
for ($i = 760; $i -le 780; $i++) {
    Write-Host "$($i+1): $($lines[$i])"
}