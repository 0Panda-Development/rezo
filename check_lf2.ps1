$b = Get-Content E:\Rezo\release\test2\test_exact.cmd -Encoding Byte
$crlf=0; $lf=0
for($i=0;$i -lt $b.Count-1;$i++){
    if($b[$i]-eq 13 -and $b[$i+1]-eq 10){$crlf++}
    elseif($b[$i]-eq 10){$lf++}
}
Write-Host "CRLF: $crlf, LF only: $lf"