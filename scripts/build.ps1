param([ValidateSet('v140','v143')][string]$Toolset='v143')
$ErrorActionPreference='Stop'
$protoRoot=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Build Tools not found. Run setup-buildtools.ps1 first.' }
$msbuild=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild is not installed.' }
& $msbuild (Join-Path $protoRoot 'PhotoMatchProto.vcxproj') /m /nologo /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$Toolset" "/flp:logfile=$protoRoot\build-$Toolset.log;verbosity=normal"
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE). See build-$Toolset.log." }
& $msbuild (Join-Path $protoRoot 'tests\ProjectionTests.vcxproj') /m /nologo /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$Toolset"
if ($LASTEXITCODE -ne 0) { throw 'Projection test compilation failed.' }
& (Join-Path $protoRoot "build\$Toolset\ProjectionTests.exe")
if ($LASTEXITCODE -ne 0) { throw 'Projection tests failed.' }
