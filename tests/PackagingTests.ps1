$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
foreach ($file in @('packaging\CheckHost.ps1','scripts\package-installer.ps1','scripts\package.ps1','scripts\register.ps1','scripts\configure-host.ps1','scripts\configure-manifest.ps1')) {
    $tokens=$null;$errors=$null
    [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $root $file),[ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw "PowerShell syntax errors in $file : $errors" }
}
# Exercise config changes in a disposable fake installation; never register COM.
$scratch=Join-Path $root ('build\packaging-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path "$scratch\scripts","$scratch\build\v143","$scratch\host\bin","$scratch\host\Config" -Force | Out-Null
foreach ($file in 'configure-host.ps1','configure-manifest.ps1') {
    Copy-Item -LiteralPath "$root\scripts\$file" -Destination "$scratch\scripts"
}
'test DLL' | Set-Content -LiteralPath "$scratch\build\v143\PhotoMatchProto.dll"
$config="<IronCAD>`r`n <AddIns>`r`n    <AddIn><siteclsid>{OTHER}</siteclsid><inprocserver>Other.dll</inprocserver></AddIn>`r`n </AddIns>`r`n</IronCAD>"
$manifest="<assembly>`r`n  <file name=`"Other.dll`"><comClass clsid=`"{OTHER}`" /></file>`r`n</assembly>"
[IO.File]::WriteAllText("$scratch\host\Config\Ironcad.Addin.config",$config)
[IO.File]::WriteAllText("$scratch\host\bin\IronCAD.AddIn.manifest",$manifest)
& "$scratch\scripts\configure-host.ps1" -IronRoot "$scratch\host"
$first=[IO.File]::ReadAllText("$scratch\host\Config\Ironcad.Addin.config")
& "$scratch\scripts\configure-host.ps1" -IronRoot "$scratch\host"
if ([IO.File]::ReadAllText("$scratch\host\Config\Ironcad.Addin.config") -cne $first) { throw 'Repeated install changed config.' }
& "$scratch\scripts\configure-host.ps1" -Unregister -IronRoot "$scratch\host"
if ([IO.File]::ReadAllText("$scratch\host\Config\Ironcad.Addin.config") -cne $config) { throw 'Removal changed other add-ins.' }
if ([IO.File]::ReadAllText("$scratch\host\bin\IronCAD.AddIn.manifest") -cne $manifest) { throw 'Removal changed other manifest entries.' }
if ([IO.File]::ReadAllText("$scratch\evidence\Ironcad.Addin.config.before-photomatch") -cne $config) { throw 'Config backup was overwritten.' }
Write-Output 'Package script syntax, repeat installation, removal, and backup checks passed.'
