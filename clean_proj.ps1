$filesToRemove = @(
    "Game\Scripts\BossTestScript",
    "Game\Scripts\CylinderEffectScript",
    "Game\Scripts\GameManagerScript",
    "Game\Scripts\HitEffectScript",
    "Game\Scripts\RingEffectScript",
    "Game\Scripts\WarningEffectScript",
    "Game\Systems\BossActionSystem",
    "Game\Systems\CombatSystem",
    "Game\Systems\EnemyAISystem",
    "Game\Systems\FluidSystem",
    "Game\Systems\HealthSystem",
    "Game\Systems\PlayerActionSystem",
    "Game\Systems\PlayerInputSystem",
    "Game\Systems\RiverSystem",
    "Game\Systems\WaveSystem",
    "Game\Systems\WeaponSystem",
    "Game\Scenes\GameOverScene",
    "Game\Scenes\GameScene",
    "Game\Scenes\TitleScene"
)

$projFile = "DirectXGameApp.vcxproj"
$filtersFile = "DirectXGameApp.vcxproj.filters"

function Clean-ProjectFile {
    param ($filePath)
    
    [xml]$xml = Get-Content $filePath
    $ns = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
    $ns.AddNamespace("ns", "http://schemas.microsoft.com/developer/msbuild/2003")

    # Remove nodes
    $nodesToRemove = @()
    foreach ($item in $xml.SelectNodes("//ns:ClCompile | //ns:ClInclude", $ns)) {
        if ($item.Include) {
            $pathNoExt = $item.Include -replace '\.(cpp|h)$', ''
            if ($filesToRemove -contains $pathNoExt) {
                $nodesToRemove += $item
            }
        }
    }
    
    foreach ($node in $nodesToRemove) {
        $node.ParentNode.RemoveChild($node) | Out-Null
    }

    # Add MainScene if not present
    if ($filePath -eq $projFile) {
        $hasMainCpp = $false
        $hasMainH = $false
        foreach ($item in $xml.SelectNodes("//ns:ClCompile", $ns)) { if ($item.Include -eq "Game\Scenes\MainScene.cpp") { $hasMainCpp = $true } }
        foreach ($item in $xml.SelectNodes("//ns:ClInclude", $ns)) { if ($item.Include -eq "Game\Scenes\MainScene.h") { $hasMainH = $true } }

        if (-not $hasMainCpp) {
            $clCompileGroup = $xml.SelectNodes("//ns:ItemGroup[ns:ClCompile]", $ns)[0]
            if ($clCompileGroup) {
                $newNode = $xml.CreateElement("ClCompile", "http://schemas.microsoft.com/developer/msbuild/2003")
                $newNode.SetAttribute("Include", "Game\Scenes\MainScene.cpp")
                $clCompileGroup.AppendChild($newNode) | Out-Null
            }
        }
        if (-not $hasMainH) {
            $clIncludeGroup = $xml.SelectNodes("//ns:ItemGroup[ns:ClInclude]", $ns)[0]
            if ($clIncludeGroup) {
                $newNode = $xml.CreateElement("ClInclude", "http://schemas.microsoft.com/developer/msbuild/2003")
                $newNode.SetAttribute("Include", "Game\Scenes\MainScene.h")
                $clIncludeGroup.AppendChild($newNode) | Out-Null
            }
        }
    }
    elseif ($filePath -eq $filtersFile) {
        $hasMainCpp = $false
        $hasMainH = $false
        foreach ($item in $xml.SelectNodes("//ns:ClCompile", $ns)) { if ($item.Include -eq "Game\Scenes\MainScene.cpp") { $hasMainCpp = $true } }
        foreach ($item in $xml.SelectNodes("//ns:ClInclude", $ns)) { if ($item.Include -eq "Game\Scenes\MainScene.h") { $hasMainH = $true } }

        if (-not $hasMainCpp) {
            $clCompileGroup = $xml.SelectNodes("//ns:ItemGroup[ns:ClCompile]", $ns)[0]
            if ($clCompileGroup) {
                $newNode = $xml.CreateElement("ClCompile", "http://schemas.microsoft.com/developer/msbuild/2003")
                $newNode.SetAttribute("Include", "Game\Scenes\MainScene.cpp")
                $filterNode = $xml.CreateElement("Filter", "http://schemas.microsoft.com/developer/msbuild/2003")
                $filterNode.InnerText = "Game\Scenes"
                $newNode.AppendChild($filterNode) | Out-Null
                $clCompileGroup.AppendChild($newNode) | Out-Null
            }
        }
        if (-not $hasMainH) {
            $clIncludeGroup = $xml.SelectNodes("//ns:ItemGroup[ns:ClInclude]", $ns)[0]
            if ($clIncludeGroup) {
                $newNode = $xml.CreateElement("ClInclude", "http://schemas.microsoft.com/developer/msbuild/2003")
                $newNode.SetAttribute("Include", "Game\Scenes\MainScene.h")
                $filterNode = $xml.CreateElement("Filter", "http://schemas.microsoft.com/developer/msbuild/2003")
                $filterNode.InnerText = "Game\Scenes"
                $newNode.AppendChild($filterNode) | Out-Null
                $clIncludeGroup.AppendChild($newNode) | Out-Null
            }
        }
    }

    $xml.Save((Join-Path (Get-Location) $filePath))
}

Clean-ProjectFile $projFile
Clean-ProjectFile $filtersFile

# Delete files from disk
foreach ($f in $filesToRemove) {
    if (Test-Path "$f.cpp") { Remove-Item "$f.cpp" -Force }
    if (Test-Path "$f.h") { Remove-Item "$f.h" -Force }
}

Write-Output "Clean completed."
