param(
    [ValidateSet("Debug", "Development", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = 'Stop'

$slnPath = Join-Path $PSScriptRoot "..\..\DirectXGame_New.sln"
$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $msbuildPath = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
} else {
    Write-Host "vswhere.exe not found." -ForegroundColor Red
    exit 1
}

if (-not $msbuildPath) {
    Write-Host "MSBuild.exe not found." -ForegroundColor Red
    exit 1
}

Write-Host "Building $Configuration configuration..." -ForegroundColor Cyan
& $msbuildPath $slnPath /p:Configuration=$Configuration /p:Platform=x64 /m /flp:Encoding=UTF-8

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

$exePath = Join-Path $PSScriptRoot "..\..\..\Generated\Outputs\$Configuration\DirectXGameApp.exe"
$exePath = [System.IO.Path]::GetFullPath($exePath)
if (Test-Path $exePath) {
    $exeDir = Split-Path $exePath -Parent
    Set-Location $exeDir
    Start-Process -FilePath $exePath -Wait -NoNewWindow
} else {
    Write-Host "Executable not found at $exePath" -ForegroundColor Red
}
