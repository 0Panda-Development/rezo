param(
    [Parameter(Mandatory)][string]$Png,
    [Parameter(Mandatory)][string]$Out
)
Add-Type -AssemblyName System.Drawing
$src = New-Object System.Drawing.Bitmap $Png
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$frames = @()
foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($src, 0, 0, $s, $s)
    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $frames += , @($s, $ms.ToArray())
    $bmp.Dispose()
    $ms.Dispose()
}
$fs = [System.IO.File]::Create($Out)
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([uint16]0)          # reserved
$bw.Write([uint16]1)          # type: icon
$bw.Write([uint16]$frames.Count)
$offset = 6 + 16 * $frames.Count

# Frame payloads: PNG for 256 (rc.exe requirement), 32-bit BGRA DIB otherwise
$payloads = @()
foreach ($f in $frames) {
    $s = $f[0]; $pngBytes = $f[1]
    $bmp = New-Object System.Drawing.Bitmap $s, $s
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($src, 0, 0, $s, $s)
    $g.Dispose()
    if ($s -eq 256) {
        $payloads += , @($s, $pngBytes)
    } else {
        $ms = New-Object System.IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Bmp)
        $bmpBytes = $ms.ToArray()
        $ms.Dispose()
        # strip BMP file header (14 bytes); keep BITMAPINFOHEADER + pixels
        $bmpBytes = [byte[]]$bmpBytes
        $dib = New-Object byte[] ($bmpBytes.Length - 14)
        [Array]::Copy($bmpBytes, 14, $dib, 0, $dib.Length)
        # icon DIBs need biHeight = 2*size (XOR bitmap + AND mask)
        [Array]::Copy([BitConverter]::GetBytes([int32]($s * 2)), 0, $dib, 8, 4)
        $maskRow = [Math]::Ceiling($s / 8.0)
        $maskRow = [Math]::Ceiling($maskRow / 4.0) * 4
        $andMask = New-Object byte[] ($maskRow * $s)
        $combined = New-Object byte[] ($dib.Length + $andMask.Length)
        [Array]::Copy($dib, 0, $combined, 0, $dib.Length)
        [Array]::Copy($andMask, 0, $combined, $dib.Length, $andMask.Length)
        $payloads += , @($s, $combined)
    }
    $bmp.Dispose()
}
foreach ($p in $payloads) {
    $s = $p[0]; $data = $p[1]
    $bw.Write([byte]($s -band 0xFF))
    $bw.Write([byte]($s -band 0xFF))
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([uint16]1)
    $bw.Write([uint16]32)
    $bw.Write([uint32]$data.Length)
    $bw.Write([uint32]$offset)
    $offset += $data.Length
}
foreach ($p in $payloads) {
    $bw.Write($p[1])
}
$bw.Flush()
$bw.Close()
"$Out written ($($frames.Count) frames)"