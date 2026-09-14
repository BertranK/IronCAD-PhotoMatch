param([switch]$Unregister,[string]$IronRoot='')
$ErrorActionPreference='Stop'
$protoRoot=Split-Path -Parent $PSScriptRoot
if (!$IronRoot) { $IronRoot=Split-Path -Parent (Split-Path -Parent $protoRoot) }
$path=Join-Path $ironRoot 'bin\IronCAD.AddIn.manifest'
$backup=Join-Path $protoRoot 'evidence\IronCAD.AddIn.manifest.before-photomatch'
$clsid='{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'
$encoding=New-Object System.Text.UTF8Encoding($false)
$original=[IO.File]::ReadAllText($path,$encoding)
[xml]$parsed=$original
$entries=@([regex]::Matches($original,'(?ms)^[ \t]*<file\b.*?</file>\r?\n?') | Where-Object {$_.Value.Contains($clsid)})
if ($entries.Count -gt 1) { throw 'Duplicate PhotoMatch manifest registrations.' }
$entry=@"
  <file name="PhotoMatchProto.dll">
    <comClass clsid="$clsid" threadingModel="Apartment" description="PhotoMatch Proto 0" progid="PhotoMatchProto.Addin" />
  </file>
"@
$entry=($entry -replace '\r?\n',"`r`n")+"`r`n"
if ($Unregister) { $entry='' }
if ($entries.Count) {
    $match=$entries[0];$updated=$original.Remove($match.Index,$match.Length).Insert($match.Index,$entry)
} elseif (!$Unregister) {
    if ($original.Contains('PhotoMatchProto.dll')) { throw 'Conflicting PhotoMatch DLL manifest entry.' }
    $position=$original.IndexOf('</assembly>');if ($position -lt 0) { throw 'Invalid add-in manifest.' }
    $updated=$original.Insert($position,$entry)
} else { $updated=$original }
[xml]$validated=$updated
if ($updated -cne $original) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
    if (!(Test-Path -LiteralPath $backup)) { [IO.File]::WriteAllBytes($backup,[IO.File]::ReadAllBytes($path)) }
    [IO.File]::WriteAllText($path,$updated,$encoding)
}
Write-Output $(if($Unregister){'PhotoMatch COM manifest entry removed.'}else{'PhotoMatch COM entry added to the dedicated IronCAD.AddIn.manifest.'})
