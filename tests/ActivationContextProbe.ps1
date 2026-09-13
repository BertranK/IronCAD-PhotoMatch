param([ValidateSet('before','after')][string]$Stage='after')
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class PhotoMatchActivationProbe {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct ActCtx {public uint size,flags;public string source;public ushort architecture,language;public string directory,resource,application;public IntPtr module;}
 [StructLayout(LayoutKind.Sequential)] struct Metadata {public IntPtr information,sectionBase;public uint sectionLength;public IntPtr metadataBase;public uint metadataLength;}
 [StructLayout(LayoutKind.Sequential)] struct KeyedData {public uint size,format;public IntPtr data;public uint length;public IntPtr global;public uint globalLength;public IntPtr sectionBase;public uint totalLength;public IntPtr actCtx;public uint roster,flags;public Metadata metadata;}
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateActCtxW(ref ActCtx data);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool ActivateActCtx(IntPtr ctx,out UIntPtr cookie);
 [DllImport("kernel32.dll")] static extern bool DeactivateActCtx(uint flags,UIntPtr cookie);
 [DllImport("kernel32.dll")] static extern void ReleaseActCtx(IntPtr ctx);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool FindActCtxSectionGuid(uint flags,IntPtr extension,uint section,ref Guid guid,ref KeyedData data);
 [DllImport("ole32.dll",PreserveSig=true)] static extern int CoCreateInstance(ref Guid clsid,IntPtr outer,uint context,ref Guid iid,out IntPtr result);
 public static string Find(string manifest,string directory) {
   ActCtx data=new ActCtx{size=(uint)Marshal.SizeOf(typeof(ActCtx)),flags=4,source=manifest,directory=directory};
   IntPtr ctx=CreateActCtxW(ref data);if(ctx==new IntPtr(-1))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
   UIntPtr cookie=UIntPtr.Zero;bool active=false;
   try {if(!(active=ActivateActCtx(ctx,out cookie)))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
     Guid guid=new Guid("A44D3379-FC03-4CBF-9B10-A8CC56B3A7E1");
     KeyedData keyed=new KeyedData{size=(uint)Marshal.SizeOf(typeof(KeyedData))};
     bool found=FindActCtxSectionGuid(0,IntPtr.Zero,4,ref guid,ref keyed);
     int error=found?0:Marshal.GetLastWin32Error();string creation="not_attempted";
     if(found){Guid iid=new Guid("2c08d687-3532-4f9b-812d-a381306d53a1");IntPtr server;int hr=CoCreateInstance(ref guid,IntPtr.Zero,1,ref iid,out server);creation="0x"+hr.ToString("X8");if(server!=IntPtr.Zero)Marshal.Release(server);}
     return "found="+found+"; win32_error="+error+"; IZAddinServer="+creation;
   }finally{if(active)DeactivateActCtx(0,cookie);ReleaseActCtx(ctx);}
 }
}
"@
$protoRoot=Split-Path -Parent $PSScriptRoot
$bin=Join-Path (Split-Path -Parent (Split-Path -Parent $protoRoot)) 'bin'
$manifest=Join-Path $bin 'IronCad.exe.manifest'
$result=[PhotoMatchActivationProbe]::Find($manifest,$bin)
$report=[ordered]@{timestamp_utc=[DateTime]::UtcNow.ToString('o');stage=$Stage;manifest=$manifest;activation_context_lookup=$result;host_initialization_proven=$false}
$report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $protoRoot "evidence\activation-$Stage.json") -Encoding utf8
$report | ConvertTo-Json

if ($Stage -eq 'after' -and $result -notmatch 'found=True; win32_error=0; IZAddinServer=0x00000000') { throw 'Activation context COM lookup or interface creation failed.' }
