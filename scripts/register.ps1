param([switch]$Unregister,[ValidateSet('v140','v143')][string]$Toolset='v143')
$ErrorActionPreference='Stop'
$protoRoot=Split-Path -Parent $PSScriptRoot
$dll=Join-Path $protoRoot "build\$Toolset\PhotoMatchProto.dll"
# Per-user registration affects only this prototype's unique CLSID.
$key='HKCU:\Software\Classes\CLSID\{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'
$appSubkey='Software\IronCAD\IRONCAD 29.0\Applications\PhotoMatchProto'
$appKey='HKCU:\'+$appSubkey
$protoClsid='{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'
if ((Test-Path -LiteralPath $appKey) -and (Get-Item -LiteralPath $appKey).GetValue('') -ne $protoClsid) {
    throw 'An unrelated application already uses the PhotoMatchProto settings name.'
}
if ($Unregister) {
    if (Get-Process -Name IronCAD -ErrorAction SilentlyContinue) { throw 'Close IronCAD after saving before unregistering the add-in.' }
    & (Join-Path $PSScriptRoot 'configure-host.ps1') -Unregister -Toolset $Toolset
    if (Test-Path -LiteralPath $key) { Remove-Item -LiteralPath $key -Recurse -Force }
    if (Test-Path -LiteralPath $appKey) { Remove-Item -LiteralPath $appKey -Recurse -Force }
    Write-Output 'PhotoMatchProto COM and IronCAD Applications registrations removed.'
    return
}
if (!(Test-Path -LiteralPath $dll)) { throw "Build first: $dll" }
if ((Test-Path -LiteralPath $key) -and (Get-Process -Name IronCAD -ErrorAction SilentlyContinue)) {
    $existing=Get-Item -LiteralPath "$key\InprocServer32" -ErrorAction SilentlyContinue
    if (!$existing -or $existing.GetValue('') -ne $dll) { throw 'Close IronCAD before changing an existing registration.' }
}
$entries=[ordered]@{
    $key='IronCAD PhotoMatch Proto 0'
    "$key\InprocServer32"=$dll
    "$key\Name"='PhotoMatchProto'
    "$key\Description"='Camera, vertex and image registration diagnostics for IronCAD 2027'
    "$key\Required Categories\{2D652377-6B3B-450e-840A-5DE09A48B464}"='IronCAD, LLC Application AddIn Site'
    "$key\Implemented Categories\{6CA5004A-8CD0-454d-88BA-E9700C9789A6}"='IRONCAD Application Addins'
}
foreach ($entry in $entries.GetEnumerator()) {
    # Registry New-Item -Force can erase child keys when a parent is recreated.
    # CreateSubKey opens an existing key without removing any of its children.
    $subkey=$entry.Key.Substring('HKCU:\'.Length)
    $handle=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($subkey)
    try { $handle.SetValue('', $entry.Value, [Microsoft.Win32.RegistryValueKind]::String) }
    finally { $handle.Dispose() }
}
New-ItemProperty -LiteralPath "$key\InprocServer32" -Name ThreadingModel -Value Apartment -PropertyType String -Force | Out-Null
foreach ($entry in $entries.GetEnumerator()) {
    if (!(Test-Path -LiteralPath $entry.Key) -or (Get-Item -LiteralPath $entry.Key).GetValue('') -ne $entry.Value) {
        throw "Registration verification failed: $($entry.Key)"
    }
}
if ((Get-Item -LiteralPath "$key\InprocServer32").GetValue('ThreadingModel') -ne 'Apartment') { throw 'ThreadingModel verification failed.' }
# IronCAD maintains its own application list in addition to COM categories.
# See SDK Samples/C#/MyFirstAddin/Addin.reg; IronCAD 2027 uses version 29.0.
$appHandle=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($appSubkey)
try {
    $appHandle.SetValue('', $protoClsid, [Microsoft.Win32.RegistryValueKind]::String)
    $appHandle.SetValue('ShowInList', 1, [Microsoft.Win32.RegistryValueKind]::DWord)
    if ($null -eq $appHandle.GetValue('LoadOnStartup')) {
        $appHandle.SetValue('LoadOnStartup', 0, [Microsoft.Win32.RegistryValueKind]::DWord)
    }
} finally { $appHandle.Dispose() }
$appRegistered=Get-Item -LiteralPath $appKey
if ($appRegistered.GetValue('') -ne $protoClsid -or $appRegistered.GetValue('ShowInList') -ne 1) {
    throw 'IronCAD Applications registration verification failed.'
}
& (Join-Path $PSScriptRoot 'configure-host.ps1') -Toolset $Toolset
Write-Output "PhotoMatchProto COM, application settings and host config registered: $dll"
Write-Output 'Enable PhotoMatchProto in IronCAD Add-in Manager. Restart IronCAD only if the entry is absent.'
