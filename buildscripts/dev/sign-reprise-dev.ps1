# SPDX-License-Identifier: GPL-3.0-only
# Local Reprise Notation development signing.
# The private key remains in the current user's Windows certificate store.
#
# Usage (from any directory):
#   & 'R:\buildscripts\dev\sign-reprise-dev.ps1'
#   & 'R:\buildscripts\dev\sign-reprise-dev.ps1' -BuildFirst
#   & 'R:\buildscripts\dev\sign-reprise-dev.ps1' -CertificateThumbprint <thumbprint>
#
# This produces an Authenticode signature; a local test certificate does not
# automatically satisfy Windows Smart App Control's trusted-publisher policy.

[CmdletBinding()]
param(
    [string]$BuildDirectory,
    [string]$CertificateThumbprint,
    [switch]$BuildFirst
)

$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw 'This script signs Windows executables and requires Windows.'
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $repositoryRoot 'builds\b'
}

if (-not (Test-Path -LiteralPath $BuildDirectory -PathType Container)) {
    throw "Build directory does not exist: $BuildDirectory"
}

if ($BuildFirst) {
    Write-Host "Building RelWithDebInfo in $BuildDirectory"
    & cmake --build $BuildDirectory --config RelWithDebInfo --parallel 4
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE. Nothing has been signed."
    }
}

$now = Get-Date
$certificates = @(Get-ChildItem 'Cert:\CurrentUser\My' -CodeSigningCert |
    Where-Object {
        $_.HasPrivateKey -and $_.NotBefore -le $now -and $_.NotAfter -gt $now -and
        (
            ($CertificateThumbprint -and $_.Thumbprint -eq ($CertificateThumbprint -replace '\s', '')) -or
            (-not $CertificateThumbprint -and $_.Subject -eq 'CN=RepriseNotation Development Test')
        )
    })

if ($certificates.Count -ne 1) {
    throw "Expected exactly one valid Reprise code-signing certificate, found $($certificates.Count). Use -CertificateThumbprint if necessary."
}

$certificate = $certificates[0]
Write-Host "Signing with $($certificate.Subject) ($($certificate.Thumbprint))"
Write-Host "Certificate expires $($certificate.NotAfter)"

$executables = @(
    (Join-Path $BuildDirectory 'RelWithDebInfo\Reprise Notation.exe'),
    (Join-Path $BuildDirectory 'install\bin\Reprise Notation.exe')
)

$found = 0
foreach ($executable in $executables) {
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        Write-Warning "Not found; skipping: $executable"
        continue
    }

    $found++
    Write-Host "Signing: $executable"
    $null = Set-AuthenticodeSignature -LiteralPath $executable -Certificate $certificate -HashAlgorithm SHA256 -ErrorAction Stop

    $verification = Get-AuthenticodeSignature -LiteralPath $executable
    if (-not $verification.SignerCertificate -or
        $verification.SignerCertificate.Thumbprint -ne $certificate.Thumbprint) {
        throw "Signature was not applied or signer does not match: $executable"
    }

    Write-Host "  Signature present; Windows verification status: $($verification.Status)"
    if ($verification.Status -ne 'Valid') {
        Write-Warning "The file is signed, but Windows does not validate its trust chain: $($verification.StatusMessage). Self-signed certificates are not sufficient for Smart App Control."
    }
}

if ($found -eq 0) {
    throw "No Reprise Notation.exe files found under $BuildDirectory. Nothing was signed."
}

Write-Host 'Development signing complete. Re-sign after every build, install, or binary replacement.'
