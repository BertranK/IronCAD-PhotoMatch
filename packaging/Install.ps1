param([string]$IronRoot='')
$ErrorActionPreference='Stop'
try {
    if (!$IronRoot) {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog=New-Object System.Windows.Forms.FolderBrowserDialog
        $dialog.Description='Choose your IronCAD 2027 folder (containing bin and Config).'
        if ($dialog.ShowDialog() -ne 'OK') { return }
        $IronRoot=$dialog.SelectedPath
    }
    $IronRoot=(Resolve-Path -LiteralPath $IronRoot).Path
    if (!(Test-Path -LiteralPath (Join-Path $IronRoot 'bin\IRONCAD.exe')) -or
        !(Test-Path -LiteralPath (Join-Path $IronRoot 'Config\Ironcad.Addin.config'))) {
        throw 'Select the IronCAD installation folder, not bin or the package folder.'
    }
    $version=(Get-Item -LiteralPath (Join-Path $IronRoot 'bin\IRONCAD.exe')).VersionInfo.FileMajorPart
    if ($version -ne 29) { throw 'This package requires IronCAD 2027 (version 29).' }
    if (Get-Process -Name IronCAD,PhotoMatch -ErrorAction SilentlyContinue) {
        throw 'Save your work and close IronCAD and PhotoMatch before installation.'
    }
    $principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) -or ![Environment]::Is64BitProcess) {
        $systemDirectory=if([Environment]::Is64BitProcess){'System32'}else{'Sysnative'}
        $powershell=Join-Path $env:SystemRoot "$systemDirectory\WindowsPowerShell\v1.0\powershell.exe"
        $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -IronRoot "'+$IronRoot+'"'
        $process=Start-Process -FilePath $powershell -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) {
            if (Test-Path -LiteralPath "$PSScriptRoot\install-error.log") { Get-Content -LiteralPath "$PSScriptRoot\install-error.log" }
            throw 'Installation failed. No restart is required; see install-error.log.'
        }
        Write-Output 'Installed. Start IronCAD and choose Add-Ins > IronCAD PhotoMatch.'
        return
    }
    $target=Join-Path $IronRoot 'bin\PhotoMatch'
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Copy-Item -Path "$PSScriptRoot\PhotoMatch\*" -Destination $target -Recurse -Force
    New-Item -ItemType Directory -Path "$PSScriptRoot\evidence" -Force | Out-Null
    & "$PSScriptRoot\scripts\register.ps1" -IronRoot $IronRoot
    if ((Get-FileHash -LiteralPath "$target\PhotoMatch.exe").Hash -ne
        (Get-FileHash -LiteralPath "$PSScriptRoot\PhotoMatch\PhotoMatch.exe").Hash) { throw 'Installed GUI verification failed.' }
} catch {
    $_.Exception.Message | Set-Content -LiteralPath "$PSScriptRoot\install-error.log" -Encoding UTF8
    Write-Error $_
    exit 1
}
