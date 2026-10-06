$b1 = Get-Content E:\Rezo\fresh_test\install.cmd -Encoding Byte
$b2 = Get-Content E:\Rezo\fresh_test\test_after_cert.cmd -Encoding Byte
$crlf1=0; $lf1=0; for($i=0;$i -lt $b1.Count-1;$i++){ if($b1[$i]-eq 13 -and $b1[$i+1]-eq 10){$crlf1++} elseif($b1[$i]-eq 10){$lf1++} }
$crlf2=0; $lf2=0; for($i=0;$i -lt $b2.Count-1;$i++){ if($b2[$i]-eq 13 -and $b2[$i+1]-eq 10){$crlf2++} elseif($b2[$i]-eq 10){$lf2++} }
Write-Host "install: CRLF=$crlf1 LF=$lf1 Size=$($b1.Count)"
Write-Host "test: CRLF=$crlf2 LF=$lf2 Size=$($b2.Count)"