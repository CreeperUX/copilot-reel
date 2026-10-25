[CmdletBinding()]
param([ValidateSet('Menu','Reel','Background','DisableStartupTask','UpdateCheck','UpdateInstall','Diagnostics')][string]$View='Menu')
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
# DisableStartupTask turns the packaged startup task off (it needs the package identity) and exits.
# UpdateCheck and UpdateInstall ask the running reel to check GitHub, or to install the update it found;
# Diagnostics writes diagnostics.txt into its data folder.
$argument=@{Menu='--settings';Reel='--preview';Background='--background';DisableStartupTask='--disable-startup-task';UpdateCheck='--update-check';UpdateInstall='--update-install';Diagnostics='--diagnostics'}[$View]
$appId=$package.PackageFamilyName+'!App'
$appProcessId=[ReelActivation]::Open($appId,$argument)
[pscustomobject]@{Version=$package.Version.ToString();View=$View;ProcessId=$appProcessId}
