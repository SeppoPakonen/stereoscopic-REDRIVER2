Add-Type -AssemblyName System.Drawing
$path = "J:\sblo\Pelit\PC\INSTALLED\Driver2\testobj_frame.bmp"
$bmp = [System.Drawing.Bitmap]::new($path)
$cnt = 0
$minX=99999; $maxX=-1; $minY=99999; $maxY=-1
for ($y=0; $y -lt $bmp.Height; $y++) {
  for ($x=0; $x -lt $bmp.Width; $x++) {
    $p = $bmp.GetPixel($x,$y)
    if (($p.R + $p.G + $p.B) -gt 0) {
      if ($cnt -lt 120) { Write-Host ("{0},{1} R={2} G={3} B={4}" -f $x,$y,$p.R,$p.G,$p.B) }
      $cnt++
      if ($x -lt $minX){$minX=$x}; if ($x -gt $maxX){$maxX=$x}
      if ($y -lt $minY){$minY=$y}; if ($y -gt $maxY){$maxY=$y}
    }
  }
}
$bmp.Dispose()
Write-Host ("total nonblack={0} bbox=({1},{2})-({3},{4})" -f $cnt,$minX,$minY,$maxX,$maxY)