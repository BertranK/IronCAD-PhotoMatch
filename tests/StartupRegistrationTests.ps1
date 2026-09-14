$ErrorActionPreference='Stop'
$script=Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts\configure-user.ps1'
$path='HKCU:\Software\PhotoMatchTests\'+[guid]::NewGuid().ToString('N')
$clsid='{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'
try {
    New-Item -Path "$path\Other" -Force | Out-Null
    Set-Item -LiteralPath "$path\Other" -Value '{OTHER}'
    New-ItemProperty -LiteralPath "$path\Other" -Name LoadOnStartup -Value 0 -PropertyType DWord | Out-Null
    & $script -ApplicationsKey $path
    $key=Get-Item -LiteralPath "$path\IronCAD PhotoMatch"
    if ($key.GetValue('') -ne $clsid -or $key.GetValue('LoadOnStartup') -ne 1 -or $key.GetValue('ShowInList') -ne 1) { throw 'Fresh install did not enable PhotoMatch.' }
    if ($key.GetValueKind('LoadOnStartup') -ne 'DWord') { throw 'Startup flag must be DWORD.' }
    New-ItemProperty -LiteralPath "$path\IronCAD PhotoMatch" -Name LoadOnStartup -Value 0 -PropertyType DWord -Force | Out-Null
    New-Item -Path "$path\PhotoMatchProto" -Force | Out-Null
    Set-Item -LiteralPath "$path\PhotoMatchProto" -Value $clsid
    & $script -ApplicationsKey $path
    & $script -ApplicationsKey $path
    if ((Get-Item -LiteralPath "$path\IronCAD PhotoMatch").GetValue('LoadOnStartup') -ne 1) { throw 'Reinstall left PhotoMatch unchecked.' }
    if (Test-Path -LiteralPath "$path\PhotoMatchProto") { throw 'Duplicate legacy entry remains.' }
    & $script -ApplicationsKey $path -Unregister
    & $script -ApplicationsKey $path -Unregister
    if (Test-Path -LiteralPath "$path\IronCAD PhotoMatch") { throw 'Removal left PhotoMatch entry.' }
    if ((Get-Item -LiteralPath "$path\Other").GetValue('LoadOnStartup') -ne 0) { throw 'Other add-in changed.' }
    New-Item -Path "$path\PhotoMatchProto" -Force | Out-Null
    Set-Item -LiteralPath "$path\PhotoMatchProto" -Value '{OTHER}'
    & $script -ApplicationsKey $path
    & $script -ApplicationsKey $path -Unregister
    if ((Get-Item -LiteralPath "$path\PhotoMatchProto").GetValue('') -ne '{OTHER}') { throw 'Unrelated legacy name was removed.' }
    Write-Output 'Startup registration: fresh install, unchecked upgrade, repeat install, removal, and unrelated entries passed.'
} finally {
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
}
