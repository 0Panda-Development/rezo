$b = Get-Content E:\Rezo\fresh_test\install.cmd -Encoding Byte
$t = [System.Text.Encoding]::UTF8.GetString($b)
$i = $t.IndexOf('if %errorlevel% equ 0')
$t.Substring($i-200, 500)