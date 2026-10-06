$b = Get-Content E:\Rezo\release\test2\install.cmd -Encoding Byte
$t = [System.Text.Encoding]::UTF8.GetString($b)
$i = $t.IndexOf('STEP 1b')
$t.Substring($i-50, 300)