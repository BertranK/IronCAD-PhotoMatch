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
$appKey='Registry::HKEY_CURRENT_USER\Software\IronCAD\IRONCAD 29.0\Applications\PhotoMatchProto'
$appRegistered=Get-Item -LiteralPath $appKey -ErrorAction SilentlyContinue
$appListed=$null -ne $appRegistered -and $appRegistered.GetValue('') -eq '{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}' -and $appRegistered.GetValue('ShowInList') -eq 1
$protoRoot=Split-Path -Parent $PSScriptRoot
$ironRoot=Split-Path -Parent (Split-Path -Parent $protoRoot)
[xml]$hostXml=Get-Content -LiteralPath (Join-Path $ironRoot 'Config\Ironcad.Addin.config') -Raw
$hostEntries=@($hostXml.IronCAD.AddIns.AddIn | Where-Object {$_.siteclsid -eq '{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'})
$hostConfigured=$hostEntries.Count -eq 1 -and $hostEntries[0].inprocserver -eq 'PhotoMatchProto.dll' -and (Test-Path -LiteralPath (Join-Path $ironRoot 'bin\PhotoMatchProto.dll'))
$report=[ordered]@{host_config_and_deployed_dll=$hostConfigured;ironcad_applications_registration=$appListed;timestamp_utc=[DateTime]::UtcNow.ToString('o');com_category_enumeration=$found;host_ui_visibility='pending';clsid='{A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1}'}
$report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot '..\evidence\registration.json') -Encoding utf8
$report | ConvertTo-Json
if (!$found) { throw 'PhotoMatchProto was not discoverable through COM category enumeration.' }

if (!$appListed) { throw 'IronCAD 29.0 Applications entry is missing or hidden.' }

if (!$hostConfigured) { throw 'Host config or deployed DLL missing.' }
