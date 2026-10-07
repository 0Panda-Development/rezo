$text = Get-Content E:\Rezo\tools\updater\updater.cs -Raw
$text = $text -creplace '\(\) =>', 'delegate()'
$text = $text -creplace '\(p, s\) =>', 'delegate(int p, string s)'
$text = $text -creplace '\$"([^"]*)"', '"$1"'
Set-Content E:\Rezo\tools\updater\updater.cs $text