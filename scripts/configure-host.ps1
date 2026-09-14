param([switch]$Unregister,[ValidateSet('v140','v143')][string]$Toolset='v143',[string]$IronRoot='')
$ErrorActionPreference='Stop'
$protoRoot=Split-Path -Parent $PSScriptRoot
if (!$IronRoot) { $IronRoot=Split-Path -Parent (Split-Path -Parent $protoRoot) }
$config=Join-Path $ironRoot 'Config\Ironcad.Addin.config'
$backup=Join-Path $protoRoot 'evidence\Ironcad.Addin.config.before-photomatch'
$clsid='{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'
$buildDll=Join-Path $protoRoot "build\$Toolset\PhotoMatchProto.dll"
$dll=Join-Path $ironRoot "bin\PhotoMatchProto.dll"
if (!(Test-Path -LiteralPath $config)) { throw "IronCAD host config not found: $config" }
$encoding=New-Object System.Text.UTF8Encoding($false)
$original=[IO.File]::ReadAllText($config,$encoding)
[xml]$parsed=$original
$pattern='(?ms)^[ \t]*<AddIn>.*?</AddIn>\r?\n?'
$entries=@([regex]::Matches($original,$pattern) | Where-Object {$_.Value.Contains($clsid)})
if ($entries.Count -gt 1) { throw 'Duplicate PhotoMatch host entries; inspect config before changing.' }
if ($Unregister) {
    $entry=''
} else {
    if (!(Test-Path -LiteralPath $buildDll)) { throw 'Build the requested DLL first.' }
    $needsCopy=!(Test-Path -LiteralPath $dll)
    if (!$needsCopy) { $needsCopy=(Get-FileHash -LiteralPath $buildDll).Hash -ne (Get-FileHash -LiteralPath $dll).Hash }
    if ($needsCopy) {
        $loaded=@(Get-Process -Name IronCAD -ErrorAction SilentlyContinue | ForEach-Object { $_.Modules } | Where-Object {$_.FileName -eq $dll})
        if ($loaded.Count) { throw 'Unload PhotoMatch or close IronCAD before replacing its deployed DLL.' }
        Copy-Item -LiteralPath $buildDll -Destination $dll -Force
    }
    $safeDll='PhotoMatchProto.dll'
    $entry=@"
    <AddIn>
      <identify></identify>
      <inprocserver>$safeDll</inprocserver>
      <siteclsid>$clsid</siteclsid>
      <modulever></modulever>
      <platformver></platformver>
      <autoload>true</autoload>
      <system>true</system>
      <displayname><default>IronCAD PhotoMatch</default></displayname>
      <description><default>Photo matching for IronCAD</default></description>
    </AddIn>
"@
    $entry=($entry -replace '\r?\n',"`r`n")+"`r`n"
}
if ($entries.Count -eq 1) {
    $match=$entries[0]
    $updated=$original.Remove($match.Index,$match.Length).Insert($match.Index,$entry)
} elseif (!$Unregister) {
    $position=$original.IndexOf(' </AddIns>')
    if ($position -lt 0) { throw 'Unexpected host config structure.' }
    $updated=$original.Insert($position,$entry)
} else { $updated=$original }
[xml]$validated=$updated
if ($updated -cne $original) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
    if (!(Test-Path -LiteralPath $backup)) { [IO.File]::WriteAllBytes($backup,[IO.File]::ReadAllBytes($config)) }
    [IO.File]::WriteAllText($config,$updated,$encoding)
}
[xml]$actual=[IO.File]::ReadAllText($config,$encoding)
$matches=@($actual.IronCAD.AddIns.AddIn | Where-Object {$_.siteclsid -eq $clsid})
if ($Unregister) {
    if ($matches.Count -ne 0) { throw 'PhotoMatch host entry removal failed.' }
} elseif ($matches.Count -ne 1 -or $matches[0].inprocserver -ne 'PhotoMatchProto.dll' -or $matches[0].autoload -ne 'true') { throw 'Host config verification failed.' }
& (Join-Path $PSScriptRoot 'configure-manifest.ps1') -Unregister:$Unregister -IronRoot $IronRoot
Write-Output $(if($Unregister){'PhotoMatch host config entry removed; other entries preserved.'}else{'PhotoMatch host config verified. Restart IronCAD to reload the application list.'})
