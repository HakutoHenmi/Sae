$content = Get-Content -Path 'Game\Scenes\MainScene.cpp' -Encoding UTF8 -Raw
$content = $content -replace 'contentH = 1650\.0f;', 'contentH = 1850.0f;'
$evaluator = {
    param($match)
    $val = [float]$match.Groups[1].Value
    if ($val -ge 680 -and $val -lt 800) { $val += 20 }
    elseif ($val -ge 800 -and $val -lt 830) { $val += 40 }
    elseif ($val -ge 830 -and $val -lt 1020) { $val += 60 }
    elseif ($val -ge 1020 -and $val -lt 1050) { $val += 80 }
    elseif ($val -ge 1050 -and $val -lt 1220) { $val += 100 }
    elseif ($val -ge 1220 -and $val -lt 1250) { $val += 120 }
    elseif ($val -ge 1250 -and $val -lt 1470) { $val += 140 }
    elseif ($val -ge 1470 -and $val -lt 1500) { $val += 160 }
    elseif ($val -ge 1500) { $val += 180 }
    return "panelY + " + ($val).ToString("F1") + "f"
}
$regex = [regex]::new('panelY \+ ([0-9]+(?:\.[0-9]+)?)f')
$newContent = $regex.Replace($content, $evaluator)
Set-Content -Path 'Game\Scenes\MainScene.cpp' -Value $newContent -Encoding UTF8
