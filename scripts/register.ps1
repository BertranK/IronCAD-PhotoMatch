param([switch]$Unregister,[ValidateSet('v140','v143')][string]$Toolset='v143',[string]$IronRoot='')
$ErrorActionPreference='Stop'
$protoRoot=Split-Path -Parent $PSScriptRoot
if (!$IronRoot) { $IronRoot=Split-Path -Parent (Split-Path -Parent $protoRoot) }
$dll=Join-Path $ironRoot 'bin\PhotoMatchProto.dll'
$clsid='{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'
$machineKey='Registry::HKEY_LOCAL_MACHINE\Software\Classes\CLSID\'+$clsid
if ($Unregister -and (Get-Process -Name IronCAD -ErrorAction SilentlyContinue)) {
    throw 'Close IronCAD after saving before unregistering the add-in.'
}
if (!$Unregister -and !(Test-Path -LiteralPath (Join-Path $protoRoot "build\$Toolset\PhotoMatchProto.dll"))) {
    throw 'Build the requested DLL first.'
}
# IronCAD returned NAME NOT FOUND for the former HKCU registration. Register
# the deployed x64 DLL through its ATL entry point in the machine COM hive.
$principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) -or ![Environment]::Is64BitProcess) {
    $systemDirectory=if([Environment]::Is64BitProcess){'System32'}else{'Sysnative'}
    $powershell=Join-Path $env:SystemRoot "$systemDirectory\WindowsPowerShell\v1.0\powershell.exe"
    $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -Toolset '+$Toolset
    if($Unregister){$arguments+=' -Unregister'}
    $arguments+=' -IronRoot "'+$IronRoot+'"'
    $process=Start-Process -FilePath $powershell -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden -PassThru -Wait
    if($process.ExitCode -ne 0){throw "Registration helper failed ($($process.ExitCode)). Run this script in administrator PowerShell for details."}
    Write-Output 'PhotoMatch registration helper completed.'
    return
}
if (!(Test-Path -LiteralPath $dll) -and $Unregister) { throw "Deployed DLL required for unregistration: $dll" }
if (!$Unregister) { & (Join-Path $PSScriptRoot 'configure-host.ps1') -Toolset $Toolset -IronRoot $IronRoot }
$arguments='/s "'+$dll+'"'
if($Unregister){$arguments='/u '+$arguments}
$process=Start-Process -FilePath (Join-Path $env:SystemRoot 'System32\regsvr32.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
if($process.ExitCode -ne 0){throw "regsvr32 failed ($($process.ExitCode))."}
if ($Unregister) {
    if(Test-Path -LiteralPath $machineKey){throw 'Machine COM registration was not removed.'}
    & (Join-Path $PSScriptRoot 'configure-host.ps1') -Unregister -Toolset $Toolset -IronRoot $IronRoot
    $legacyKey='HKCU:\Software\IronCAD\IRONCAD 29.0\Applications\PhotoMatchProto'
    if((Test-Path -LiteralPath $legacyKey) -and (Get-Item -LiteralPath $legacyKey).GetValue('') -eq $clsid){
        Remove-Item -LiteralPath $legacyKey -Recurse -Force
    }
    Write-Output 'PhotoMatch COM and host entries removed; deployed DLL retained.'
} else {
    $registered=Get-Item -LiteralPath "$machineKey\InprocServer32"
    if($registered.GetValue('') -ne $dll -or $registered.GetValue('ThreadingModel') -ne 'Apartment'){
        throw 'Machine COM registration verification failed.'
    }
    Write-Output "Registered x64 PhotoMatch DLL: $dll"
    Write-Output 'PhotoMatch is configured to load by default. After restarting IronCAD, use Add-Ins > IronCAD PhotoMatch.'
}
