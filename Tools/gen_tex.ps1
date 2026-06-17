Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap 256, 256
for($y=0; $y -lt 256; $y++){
    for($x=0; $x -lt 256; $x++){
        $dx = $x + 0.5 - 128.0
        $dy = $y + 0.5 - 128.0
        $dist = [Math]::Sqrt($dx*$dx + $dy*$dy)
        $a = 0
        if($dist -le 128.0){
            $n = $dist / 128.0
            $a_f = 1.0 - $n*$n
            $a_f = $a_f * $a_f * $a_f  # cubic falloff for very soft edge
            $a = [int]($a_f * 255.0)
        }
        $color = [System.Drawing.Color]::FromArgb($a, 255, 255, 255)
        $bmp.SetPixel($x, $y, $color)
    }
}
$bmp.Save("Resources\Textures\soft_circle.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "soft_circle.png created."
