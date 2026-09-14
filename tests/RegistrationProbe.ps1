$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
[ComImport, Guid("0002E000-0000-0000-C000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IPhotoMatchEnumGuid {
    [PreserveSig] int Next(uint count, out Guid value, out uint fetched);
    void Skip(uint count);
    void Reset();
    void Clone(out IPhotoMatchEnumGuid result);
}
[ComImport, Guid("0002E013-0000-0000-C000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IPhotoMatchCatInformation {
    void EnumCategories(uint lcid, out IntPtr result);
    void GetCategoryDesc(ref Guid id, uint lcid, out IntPtr result);
    void EnumClassesOfCategories(uint implementedCount, [MarshalAs(UnmanagedType.LPArray,SizeParamIndex=0)] Guid[] implemented,
        uint requiredCount, [MarshalAs(UnmanagedType.LPArray,SizeParamIndex=2)] Guid[] required, out IPhotoMatchEnumGuid result);
}
public static class PhotoMatchRegistrationProbe {
    public static bool Find() {
        object raw=Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("0002E005-0000-0000-C000-000000000046")));
        IPhotoMatchEnumGuid enumerator=null;
        try {
            var info=(IPhotoMatchCatInformation)raw;
            info.EnumClassesOfCategories(1,new[]{new Guid("6CA5004A-8CD0-454d-88BA-E9700C9789A6")},
                1,new[]{new Guid("2D652377-6B3B-450e-840A-5DE09A48B464")},out enumerator);
            Guid found;uint count;Guid target=new Guid("A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1");
            while(true) {
                int hr=enumerator.Next(1,out found,out count);
                if(hr<0)Marshal.ThrowExceptionForHR(hr);
                if(count==0)return false;
                if(found==target)return true;
            }
        } finally {
            if(enumerator!=null)Marshal.ReleaseComObject(enumerator);
            Marshal.ReleaseComObject(raw);
        }
    }
}
"@
$found=[PhotoMatchRegistrationProbe]::Find()
$protoRoot=Split-Path -Parent $PSScriptRoot
$ironRoot=Split-Path -Parent (Split-Path -Parent $protoRoot)
[xml]$hostXml=Get-Content -LiteralPath (Join-Path $ironRoot 'Config\Ironcad.Addin.config') -Raw
$hostEntries=@($hostXml.IronCAD.AddIns.AddIn | Where-Object {$_.siteclsid -eq '{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'})
$hostConfigured=$hostEntries.Count -eq 1 -and $hostEntries[0].inprocserver -eq 'PhotoMatchProto.dll' -and (Test-Path -LiteralPath (Join-Path $ironRoot 'bin\PhotoMatchProto.dll'))
$autoLoadConfigured=$hostEntries.Count -eq 1 -and $hostEntries[0].autoload -eq 'true'
$machineKey='Registry::HKEY_LOCAL_MACHINE\Software\Classes\CLSID\{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}\InprocServer32'
$registered=Get-Item -LiteralPath $machineKey -ErrorAction SilentlyContinue
$machineRegistered=$null -ne $registered -and $registered.GetValue('') -eq (Join-Path $ironRoot 'bin\PhotoMatchProto.dll') -and $registered.GetValue('ThreadingModel') -eq 'Apartment'
$hostModules=@(Get-Process -Name IronCAD -ErrorAction SilentlyContinue | ForEach-Object {
    $hostProcess=$_
    $hostProcess.Modules | Where-Object ModuleName -eq 'PhotoMatchProto.dll' | ForEach-Object {
        [ordered]@{pid=$hostProcess.Id;path=$_.FileName}
    }
})
$report=[ordered]@{host_config_and_deployed_dll=$hostConfigured;autoload_configured=$autoLoadConfigured;machine_com_registration=$machineRegistered;timestamp_utc=[DateTime]::UtcNow.ToString('o');com_category_enumeration=$found;host_modules=$hostModules;host_ui_visibility='requires_screen_verification';clsid='{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'}
New-Item -ItemType Directory -Path (Join-Path $PSScriptRoot '..\evidence') -Force | Out-Null
$report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot '..\evidence\registration.json') -Encoding utf8
$report | ConvertTo-Json
if (!$found) { throw 'PhotoMatchProto was not discoverable through COM category enumeration.' }

if (!$machineRegistered) { throw 'Machine x64 COM registration is missing or points to the wrong DLL.' }

if (!$hostConfigured) { throw 'Host config or deployed DLL missing.' }
if (!$autoLoadConfigured) { throw 'PhotoMatch default loading is not configured.' }
