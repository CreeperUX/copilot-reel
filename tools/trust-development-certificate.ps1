param(
    [Parameter(Mandatory=$true)][string]$CertificatePath,
    [Parameter(Mandatory=$true)][string]$ExpectedThumbprint,
    [Parameter(Mandatory=$true)][string]$StatusPath
)
$ErrorActionPreference='Stop'
try {
    $cert=[System.Security.Cryptography.X509Certificates.X509Certificate2]::new($CertificatePath)
    if($cert.Thumbprint -ne $ExpectedThumbprint -or $cert.Subject -ne 'CN=CopilotReel Development') {throw 'Certificate identity does not match the approved development signer.'}
    $result=Import-Certificate -FilePath $CertificatePath -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople'
    if($result.Thumbprint -ne $ExpectedThumbprint) {throw 'Certificate import did not return the expected identity.'}
    [pscustomobject]@{Success=$true;Thumbprint=$result.Thumbprint;Store='LocalMachine\TrustedPeople'} | ConvertTo-Json | Set-Content -LiteralPath $StatusPath -Encoding utf8
} catch {
    [pscustomobject]@{Success=$false;Error=$_.Exception.Message} | ConvertTo-Json | Set-Content -LiteralPath $StatusPath -Encoding utf8
    exit 1
}
