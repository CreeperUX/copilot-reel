[CmdletBinding()]
param([ValidateSet('Menu','Reel','Background')][string]$View='Menu')
$ErrorActionPreference='Stop'
$package=Get-AppxPackage -Name 'CopilotReel.Desktop'
if(-not $package) {throw 'Copilot Reel is not installed for the current user.'}
if(-not ('ReelActivation' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
[ComImport, Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C")]
class ReelActivationManager { }
[ComImport, Guid("2E941141-7F97-4756-BA1D-9DECDE894A3D"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IReelActivationManager {
    [PreserveSig] int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)] string appId,
        [MarshalAs(UnmanagedType.LPWStr)] string arguments, uint options, out uint processId);
}
public static class ReelActivation {
    public static uint Open(string appId, string arguments) {
        var manager=(IReelActivationManager)new ReelActivationManager();
        try {
            uint processId;
            int result=manager.ActivateApplication(appId,arguments,0,out processId);
            Marshal.ThrowExceptionForHR(result);
            return processId;
        } finally { Marshal.FinalReleaseComObject(manager); }
    }
}
'@
}
$argument=@{Menu='--settings';Reel='--preview';Background='--background'}[$View]
$appId=$package.PackageFamilyName+'!App'
$appProcessId=[ReelActivation]::Open($appId,$argument)
[pscustomobject]@{Version=$package.Version.ToString();View=$View;ProcessId=$appProcessId}
