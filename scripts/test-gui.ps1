$ErrorActionPreference='Stop'
$protoRoot=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if(!$msbuild){throw 'MSBuild not found.'}
& $msbuild (Join-Path $protoRoot 'tests\PipeHarness.vcxproj') /m /nologo /p:Configuration=Release /p:Platform=x64
if($LASTEXITCODE -ne 0){throw 'Pipe harness build failed.'}
Push-Location $protoRoot
try {
    & (Join-Path $protoRoot '.venv\Scripts\python.exe') -m unittest discover -s tests -p 'test_*.py'
    if($LASTEXITCODE -ne 0){throw 'Python/pipe tests failed.'}
    & node --test tests/image-coordinates.test.cjs
    if($LASTEXITCODE -ne 0){throw 'Photo coordinate tests failed.'}
} finally {Pop-Location}
