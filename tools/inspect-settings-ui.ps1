# Read-only UI Automation snapshot of the Windows Settings process.
# Run with Windows PowerShell 5.1, which provides UIAutomationClient.
param([string]$OutputPath)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$settings=Get-Process -Name SystemSettings -ErrorAction Stop | Select-Object -First 1
$condition=[System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ProcessIdProperty,$settings.Id)
$root=[System.Windows.Automation.AutomationElement]::RootElement.FindFirst([System.Windows.Automation.TreeScope]::Children,$condition)
if(-not $root) {
    # Settings may be hosted in ApplicationFrameHost rather than its own top-level HWND.
    $nameCondition=[System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty,'Settings')
    $root=[System.Windows.Automation.AutomationElement]::RootElement.FindFirst([System.Windows.Automation.TreeScope]::Children,$nameCondition)
}
if(-not $root) {throw 'Settings window is not available to UI Automation.'}
$all=$root.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)
$items=@(foreach($element in $all) {
    try {
        $current=$element.Current
        if($current.Name -or $current.AutomationId) {
            [pscustomobject]@{Name=$current.Name;Id=$current.AutomationId;Type=$current.ControlType.ProgrammaticName;Enabled=$current.IsEnabled;Offscreen=$current.IsOffscreen}
        }
    } catch {}
})
$json=$items | ConvertTo-Json -Depth 3
if($OutputPath) {$json | Set-Content -LiteralPath $OutputPath -Encoding utf8}
$json
