$ErrorActionPreference='Stop'
$package=Get-AppxPackage -Name 'CopilotReel.Desktop'
if(-not $package) {throw 'CopilotReel.Desktop is not registered for the current user.'}
$manifest=Get-AppxPackageManifest -Package $package.PackageFullName
$provider=@($manifest.SelectNodes('//*[local-name()="AppExtension"]') | Where-Object {$_.GetAttribute('Name') -eq 'com.microsoft.windows.copilotkeyprovider'})
if($provider.Count -ne 1) {throw 'Expected exactly one Copilot key provider extension.'}
$application=$provider[0].ParentNode.ParentNode.ParentNode
$properties=$provider[0].SelectSingleNode('./*[local-name()="Properties"]')
foreach($item in @(@('SingleTap','0','Tap'),@('PressAndHoldStart','1','Down'),@('PressAndHoldStop','2','Up'))) {
    $node=$properties.SelectSingleNode('./*[local-name()="'+$item[0]+'"]')
    if(-not $node -or $node.GetAttribute('MessageWParam') -ne $item[1] -or $node.InnerText -ne ('copilot-reel://?state='+$item[2])) {throw "Invalid state declaration: $($item[0])"}
}
$selection=Get-ItemProperty -LiteralPath 'HKCU:\Software\Microsoft\Windows\Shell\BrandedKey' -ErrorAction SilentlyContinue
$aumid=$package.PackageFamilyName+'!'+$application.GetAttribute('Id')
[pscustomobject]@{
    Package=$package.PackageFullName
    SignatureKind=[string]$package.SignatureKind
    DevelopmentMode=$package.IsDevelopmentMode
    AUMID=$aumid
    DisplayName=$provider[0].GetAttribute('DisplayName')
    ProviderDeclarationValid=$true
    StateDeclarationsValid=$true
    SelectedByUser=($selection.BrandedKeyChoiceType -eq 'App' -and $selection.AppAumid -eq $aumid)
    PickerVisibility='Must be verified in Windows Settings; declaration alone is not proof.'
} | ConvertTo-Json
