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

Write-Host "Cleaning $Configuration configuration..." -ForegroundColor Yellow
& $msbuildPath $slnPath /t:Clean /p:Configuration=$Configuration /p:Platform=x64 /m /flp:Encoding=UTF-8

if ($LASTEXITCODE -ne 0) {
    Write-Host "Clean failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Clean completed. Building..." -ForegroundColor Cyan
& $msbuildPath $slnPath /p:Configuration=$Configuration /p:Platform=x64 /m /flp:Encoding=UTF-8

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Clean & Build succeeded." -ForegroundColor Green
