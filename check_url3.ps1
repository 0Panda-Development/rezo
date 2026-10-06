$bytes = Get-Content E:\Rezo\release\RezoUpdater.exe -Encoding Byte
$text = [System.Text.Encoding]::ASCII.GetString($bytes)
$i = $text.IndexOf('https://github.com')
if ($i -ge 0) { $text.Substring($i, 100) } else { "NOT FOUND" }