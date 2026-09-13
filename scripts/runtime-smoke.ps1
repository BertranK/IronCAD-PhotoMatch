$ErrorActionPreference='Stop'
$protoRoot=Split-Path -Parent $PSScriptRoot
$sdkRoot=Split-Path -Parent $protoRoot
$evidence=Join-Path $protoRoot 'evidence'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$report=[ordered]@{timestamp_utc=[DateTime]::UtcNow.ToString('o');status='started';stage='attach to running IronCAD';api_read=$null;error=$null;process=@(Get-Process IronCAD -ErrorAction SilentlyContinue | Select-Object Id,MainWindowTitle,Responding)}
try {
    $refs=@((Join-Path $sdkRoot 'Samples\C#\References\IronCADCOMInterop.dll'),(Join-Path $sdkRoot 'Samples\C#\References\interop.ICApiIronCAD.dll'))
    foreach ($ref in $refs) { Add-Type -Path $ref }
    if (!('PhotoMatchReadProbe' -as [type])) {
        Add-Type -ReferencedAssemblies $refs -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using IronCADCOMInterop;
using interop.ICApiIronCAD;
public static class PhotoMatchReadProbe {
    [DllImport("ole32.dll", CharSet=CharSet.Unicode, PreserveSig=false)]
    static extern void CLSIDFromProgID(string name, out Guid id);
    [DllImport("oleaut32.dll", PreserveSig=false)]
    static extern void GetActiveObject(ref Guid id, IntPtr reserved, [MarshalAs(UnmanagedType.IUnknown)] out object result);
    public static string Read() {
        Guid id; CLSIDFromProgID("IronCAD.Application",out id);
        object raw; GetActiveObject(ref id,IntPtr.Zero,out raw);
        IIronCADApp iron=(IIronCADApp)raw;
        IZBaseApp app=(IZBaseApp)iron.ZIronCADApp;
        IZDoc doc=(IZDoc)app.ActiveDoc;
        IZSceneDoc scene=(IZSceneDoc)doc;
        IZCameraMgr mgr=(IZCameraMgr)scene.CameraMgr;
        IZCamera camera=(IZCamera)mgr.ActiveCamera;
        return "API="+app.ApiVersion+"; Doc="+doc.Name+"; Perspective="+camera.Perspective+"; FOV="+camera.FieldOfView;
    }
}
"@
    }
    $report.stage='read current API and camera (no mutation)'
    $report.api_read=[PhotoMatchReadProbe]::Read()
    $report.status='api_connected'
} catch {
    $report.status='blocked'
    $report.error=$_.Exception.ToString()
} finally {
    $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $evidence 'runtime-smoke.json') -Encoding UTF8
    $report | ConvertTo-Json -Depth 6
}
if ($report.status -ne 'api_connected') { exit 1 }
