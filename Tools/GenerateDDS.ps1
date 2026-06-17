$file = 'Resources/Textures/skybox.dds'
$fs = [System.IO.File]::Create($file)
$bw = New-Object System.IO.BinaryWriter $fs

# Magic "DDS "
$bw.Write([byte]0x44); $bw.Write([byte]0x44); $bw.Write([byte]0x53); $bw.Write([byte]0x20)
# Size
$bw.Write([uint32]124)
# Flags: DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
$bw.Write([uint32]0x1007)
# Height
$bw.Write([uint32]256)
# Width
$bw.Write([uint32]256)
# PitchOrLinearSize
$bw.Write([uint32](256 * 4))
# Depth
$bw.Write([uint32]0)
# MipMapCount
$bw.Write([uint32]1)
# Reserved1 (11 DWORDs)
for ($i = 0; $i -lt 11; $i++) { $bw.Write([uint32]0) }

# PixelFormat
$bw.Write([uint32]32) # Size
$bw.Write([uint32]0x41) # Flags (DDPF_RGB | DDPF_ALPHAPIXELS)
$bw.Write([uint32]0) # FourCC
$bw.Write([uint32]32) # RGBBitCount
$bw.Write([uint32]0x00FF0000) # RBitMask (Windows is often BGRA, let's use standard RGB layout where R is highest byte or lowest byte? DirectX is usually BGRA or RGBA. Let's write R, G, B, A natively and map it using RGBA 0x000000FF)
$bw.Write([uint32]0x0000FF00) # GBitMask
$bw.Write([uint32]0x000000FF) # BBitMask
$bw.Write([uint32]0xFF000000) # ABitMask

# Caps1: DDSCAPS_COMPLEX | DDSCAPS_TEXTURE
$bw.Write([uint32]0x1008)
# Caps2: CUBEMAP + all faces
$bw.Write([uint32]0xFE00)
# Caps3, Caps4, Reserved2
$bw.Write([uint32]0); $bw.Write([uint32]0); $bw.Write([uint32]0)

# Colors
$skyR = 50; $skyG = 100; $skyB = 200
$horR = 200; $horG = 220; $horB = 255
$grndR = 80; $grndG = 80; $grndB = 80

# 6 faces
for ($f = 0; $f -lt 6; $f++) {
    for ($y = 0; $y -lt 256; $y++) {
        for ($x = 0; $x -lt 256; $x++) {
            $r = 0; $g = 0; $b = 0; $a = 255
            if ($f -eq 2) { # Top
                $r = $skyR; $g = $skyG; $b = $skyB
            } elseif ($f -eq 3) { # Bottom
                $r = $grndR; $g = $grndG; $b = $grndB
            } else { # Sides
                # y=0 is top, y=255 is bottom
                $t = $y / 255.0
                $r = $skyR * (1 - $t) + $horR * $t
                $g = $skyG * (1 - $t) + $horG * $t
                $b = $skyB * (1 - $t) + $horB * $t
            }
            # Write B, G, R, A (assuming BGRA format since we set RBitMask=FF0000)
            $bw.Write([byte]$b); $bw.Write([byte]$g); $bw.Write([byte]$r); $bw.Write([byte]$a)
        }
    }
}

$bw.Close()
$fs.Close()
