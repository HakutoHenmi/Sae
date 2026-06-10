$file = 'Resources/Textures/skybox_custom.dds'
$fs = [System.IO.File]::Create($file)
$bw = New-Object System.IO.BinaryWriter $fs

# Magic "DDS "
$bw.Write([byte]0x44); $bw.Write([byte]0x44); $bw.Write([byte]0x53); $bw.Write([byte]0x20)
$bw.Write([uint32]124)
$bw.Write([uint32]0x1007)
$bw.Write([uint32]256)
$bw.Write([uint32]256)
$bw.Write([uint32]1024)
$bw.Write([uint32]0)
$bw.Write([uint32]1)
for ($i = 0; $i -lt 11; $i++) { $bw.Write([uint32]0) }

$bw.Write([uint32]32)
$bw.Write([uint32]0x41)
$bw.Write([uint32]0)
$bw.Write([uint32]32)
$bw.Write([uint32]16711680) # 0x00FF0000
$bw.Write([uint32]65280)    # 0x0000FF00
$bw.Write([uint32]255)      # 0x000000FF
$bw.Write([uint32]4278190080) # 0xFF000000

$bw.Write([uint32]0x1008)
$bw.Write([uint32]0xFE00)
$bw.Write([uint32]0); $bw.Write([uint32]0); $bw.Write([uint32]0)

$skyR = 50; $skyG = 100; $skyB = 200
$horR = 200; $horG = 220; $horB = 255
$grndR = 80; $grndG = 80; $grndB = 80

for ($f = 0; $f -lt 6; $f++) {
    for ($y = 0; $y -lt 256; $y++) {
        for ($x = 0; $x -lt 256; $x++) {
            $r = 0; $g = 0; $b = 0; $a = 255
            if ($f -eq 2) { 
                $r = $skyR; $g = $skyG; $b = $skyB
            } elseif ($f -eq 3) { 
                $r = $grndR; $g = $grndG; $b = $grndB
            } else { 
                $t = $y / 255.0
                $r = $skyR * (1 - $t) + $horR * $t
                $g = $skyG * (1 - $t) + $horG * $t
                $b = $skyB * (1 - $t) + $horB * $t
            }
            $bw.Write([byte]$b); $bw.Write([byte]$g); $bw.Write([byte]$r); $bw.Write([byte]$a)
        }
    }
}
$bw.Close(); $fs.Close()
