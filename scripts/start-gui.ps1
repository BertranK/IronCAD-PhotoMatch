param([int]$HostProcessId=0)
$ErrorActionPreference='Stop'
$protoRoot=Split-Path -Parent $PSScriptRoot
$python=Join-Path $protoRoot '.venv\Scripts\pythonw.exe'
if(!(Test-Path -LiteralPath $python)){throw 'Run scripts/setup-gui.ps1 first.'}
$arguments='"'+(Join-Path $protoRoot 'gui\app.py')+'"'
if($HostProcessId){$arguments+=' --host '+$HostProcessId}
Start-Process -FilePath $python -ArgumentList $arguments -WorkingDirectory $protoRoot
