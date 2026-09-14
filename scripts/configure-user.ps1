param([switch]$Unregister,[string]$ApplicationsKey='HKCU:\Software\IronCAD\IRONCAD 29.0\Applications')
$ErrorActionPreference='Stop'
$clsid='{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'
$key=Join-Path $ApplicationsKey 'IronCAD PhotoMatch'
if (!$Unregister) {
    if ((Test-Path -LiteralPath $key) -and (Get-Item -LiteralPath $key).GetValue('') -ne $clsid) {
        throw 'The IronCAD PhotoMatch application name is registered to another add-in.'
    }
    if (!(Test-Path -LiteralPath $key)) { New-Item -Path $key -Force | Out-Null }
    Set-Item -LiteralPath $key -Value $clsid
    foreach ($name in 'LoadOnStartup','ShowInList') {
        New-ItemProperty -LiteralPath $key -Name $name -Value 1 -PropertyType DWord -Force | Out-Null
    }
}
# Remove only entries owned by PhotoMatch, including the former prototype name.
$names=if ($Unregister) { @('IronCAD PhotoMatch','PhotoMatchProto') } else { @('PhotoMatchProto') }
foreach ($name in $names) {
    $entry=Join-Path $ApplicationsKey $name
    if ((Test-Path -LiteralPath $entry) -and (Get-Item -LiteralPath $entry).GetValue('') -eq $clsid) {
        Remove-Item -LiteralPath $entry -Recurse -Force
    }
}
