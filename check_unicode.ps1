$bytes = Get-Content E:\Rezo\release\RezoUpdater_downloaded.exe -Encoding Byte
$text = [System.Text.Encoding]::Unicode.GetString($bytes)
$i = $text.IndexOf('github.com')
if ($i -ge 0) { $text.Substring($i, 100) } else { "NOT FOUND in Unicode" }