param([Parameter(Mandatory=$true)][string]$IronRoot,[string]$ErrorFile='')
$ErrorActionPreference='Stop'
try {
    $exe=Join-Path $IronRoot 'bin\IRONCAD.exe'
    if (!(Test-Path -LiteralPath $exe) -or (Get-Item -LiteralPath $exe).VersionInfo.FileMajorPart -ne 29) {
        throw 'Choose the IronCAD 2027 installation folder containing bin and Config.'
    }
    foreach ($file in 'Config\Ironcad.Addin.config','bin\IronCAD.AddIn.manifest') {
        [xml]$document=[IO.File]::ReadAllText((Join-Path $IronRoot $file))
    }
    if (Get-Process -Name IronCAD,PhotoMatch -ErrorAction SilentlyContinue) {
        throw 'Save your work and close IronCAD and PhotoMatch, then try again.'
    }
} catch {
    if ($ErrorFile) { [IO.File]::WriteAllText($ErrorFile,$_.Exception.Message) }
    Write-Error $_ -ErrorAction Continue
    exit 1
}
