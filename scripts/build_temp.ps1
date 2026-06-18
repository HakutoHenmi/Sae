$slnPath = "c:\Users\k024g\source\repos\Sae\DirectXGame_New.sln"
$msbuildPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuildPath $slnPath /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /m /flp:Encoding=UTF-8
