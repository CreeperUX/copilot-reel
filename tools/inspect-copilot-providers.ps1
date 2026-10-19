# Read-only verification of the current user's Copilot-key provider packages.
# Does not change package registration or registry values.
$providerName = 'com.microsoft.windows.copilotkeyprovider'
$packages = @(Get-AppxPackage)
$errors = @()
$providers = @(foreach ($package in $packages) {
    try {
        $manifest = Get-AppxPackageManifest -Package $package.PackageFullName -ErrorAction Stop
        foreach ($extension in $manifest.SelectNodes('//*[local-name()="AppExtension"]')) {
            if ($extension.GetAttribute('Name') -ne $providerName) { continue }
            $application = $extension.ParentNode.ParentNode.ParentNode
            [pscustomobject]@{
                PackageFullName = $package.PackageFullName
                PackageFamilyName = $package.PackageFamilyName
                SignatureKind = [string]$package.SignatureKind
                ApplicationId = $application.GetAttribute('Id')
                AUMID = $package.PackageFamilyName + '!' + $application.GetAttribute('Id')
                Executable = $application.GetAttribute('Executable')
                ExtensionXml = $extension.OuterXml
            }
        }
    }
    catch { $errors += [pscustomobject]@{ Package = $package.PackageFullName; Error = $_.Exception.Message } }
})
[pscustomobject]@{
    CollectedAt = (Get-Date).ToString('o')
    CurrentUserPackageCount = $packages.Count
    ManifestErrors = $errors
    ProviderCount = $providers.Count
    Providers = $providers
} | ConvertTo-Json -Depth 6
