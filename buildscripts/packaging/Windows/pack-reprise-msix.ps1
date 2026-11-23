# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InstallDirectory,
    [Parameter(Mandatory)][string]$OutputPackage,
    [string]$PackageName,
    [string]$Publisher,
    [string]$PublisherDisplayName,
    [ValidatePattern('^\d+\.\d+\.\d+\.0$')][string]$PackageVersion,
    [Parameter(Mandatory)][string]$MakeAppxPath,
    [ValidatePattern('^[a-f0-9]{40}$')][string]$SourceRevision,
    [string]$QtNoticesDirectory,
    [string]$CertificateThumbprint
)

$ErrorActionPreference = 'Stop'
[xml]$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'AppxManifest.xml') -Raw
$identity = $manifest.Package.Identity
if ($PackageName) { $identity.SetAttribute('Name', $PackageName) }
if ($Publisher) { $identity.SetAttribute('Publisher', $Publisher) }
if ($PackageVersion) { $identity.SetAttribute('Version', $PackageVersion) }
if ($PublisherDisplayName) { $manifest.Package.Properties.PublisherDisplayName = $PublisherDisplayName }

$repository = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..\..')).Path
$install = (Resolve-Path -LiteralPath $InstallDirectory).Path
$makeAppx = (Resolve-Path -LiteralPath $MakeAppxPath).Path
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPackage)

if ((Test-Path -LiteralPath $output) -or [IO.Path]::GetExtension($output) -ne '.msix') {
    throw 'Choose a new output file with the .msix extension.'
}

$executable = Get-Item -LiteralPath (Join-Path $install 'bin\Reprise Notation.exe')
if (!(Test-Path -LiteralPath (Join-Path $install 'bin\Qt6Core.dll')) -or
    !(Test-Path -LiteralPath (Join-Path $install 'bin\platforms\qwindows.dll'))) {
    throw 'Deploy the release Qt runtime to bin with windeployqt before packaging.'
}
$stream = [IO.File]::OpenRead($executable.FullName)
try {
    $reader = [IO.BinaryReader]::new($stream)
    $stream.Position = 0x3c
    $stream.Position = $reader.ReadInt32()
    if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664) {
        throw 'This packager requires an x64 Reprise build.'
    }
} finally {
    $stream.Dispose()
}

if ($CertificateThumbprint) {
    $certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$CertificateThumbprint"
    if (!$certificate.HasPrivateKey -or $certificate.Subject -cne $identity.Publisher) {
        throw 'The signing certificate must have a private key and match the package Publisher.'
    }
    $signTool = (Resolve-Path -LiteralPath (Join-Path (Split-Path $makeAppx) 'signtool.exe')).Path
}

$temp = (Resolve-Path -LiteralPath ([IO.Path]::GetTempPath())).Path
$stage = Join-Path $temp "RepriseMsix-$([guid]::NewGuid())"
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    New-Item -ItemType Directory -Path (Join-Path $stage 'bin'), (Join-Path $stage 'Assets') | Out-Null
    Copy-Item -LiteralPath $executable.FullName -Destination (Join-Path $stage 'bin')
    Get-ChildItem -LiteralPath (Join-Path $install 'bin') -Filter '*.dll' -File |
        Copy-Item -Destination (Join-Path $stage 'bin')
    Get-ChildItem -LiteralPath (Join-Path $install 'bin') -Directory |
        Copy-Item -Destination (Join-Path $stage 'bin') -Recurse

    foreach ($directory in @('extensions', 'fonts', 'locale', 'plugins', 'sound', 'styles', 'tables', 'templates',
                             'wallpapers', 'workspaces')) {
        $source = Join-Path $install $directory
        if (Test-Path -LiteralPath $source -PathType Container) {
            Copy-Item -LiteralPath $source -Destination $stage -Recurse
        }
    }
    Copy-Item -LiteralPath (Join-Path $repository 'LICENSE.txt') -Destination $stage
    $distributionNotice = Join-Path $stage 'REPRISE-DISTRIBUTION.md'
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'REPRISE-DISTRIBUTION.md') -Destination $distributionNotice
    if ($SourceRevision) {
        $sourceUrl = "https://github.com/emanuelecampilongo/reprise-notation/tree/$SourceRevision"
        Add-Content -LiteralPath $distributionNotice -Value "`nCorresponding Reprise Notation source for this package is available at $sourceUrl."
    }
    if ($QtNoticesDirectory) {
        $qtNotices = (Resolve-Path -LiteralPath $QtNoticesDirectory).Path
        $qtDestination = Join-Path $stage 'Licenses\Qt'
        New-Item -ItemType Directory -Path $qtDestination -Force | Out-Null
        Get-ChildItem -LiteralPath $qtNotices -File | Copy-Item -Destination $qtDestination
    }
    $fontRoot = Join-Path $repository 'fonts'
    Get-ChildItem -LiteralPath $fontRoot -File -Recurse | Where-Object Name -match '^(LICENSE|OFL)(\.txt)?$' |
        ForEach-Object {
            $relative = $_.FullName.Substring($fontRoot.Length + 1)
            $notice = Join-Path (Join-Path $stage 'Licenses\Fonts') $relative
            New-Item -ItemType Directory -Path (Split-Path $notice) -Force | Out-Null
            Copy-Item -LiteralPath $_.FullName -Destination $notice
        }

    Copy-Item -LiteralPath (Join-Path $repository 'share\icons\AppIcon\RepriseAppIcon.ico') -Destination (Join-Path $stage 'Assets')
    Add-Type -AssemblyName System.Drawing
    $icon = [Drawing.Image]::FromFile((Join-Path $repository 'share\icons\AppIcon\RepriseAppIcon_128x128.png'))
    try {
        foreach ($size in @(44, 50, 150)) {
            $bitmap = [Drawing.Bitmap]::new($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                $graphics = [Drawing.Graphics]::FromImage($bitmap)
                try {
                    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                    $graphics.DrawImage($icon, 0, 0, $size, $size)
                } finally {
                    $graphics.Dispose()
                }
                $bitmap.Save((Join-Path $stage "Assets\Logo$size.png"), [Drawing.Imaging.ImageFormat]::Png)
            } finally {
                $bitmap.Dispose()
            }
        }
    } finally {
        $icon.Dispose()
    }

    $manifest.Save((Join-Path $stage 'AppxManifest.xml'))

    & $makeAppx pack /d $stage /p $output
    if ($LASTEXITCODE -ne 0) { throw "MakeAppx failed with exit code $LASTEXITCODE." }

    if ($CertificateThumbprint) {
        & $signTool sign /fd SHA256 /sha1 $CertificateThumbprint $output
        if ($LASTEXITCODE -ne 0) { throw "SignTool failed with exit code $LASTEXITCODE. The package is not signed." }
    }
} finally {
    $resolvedStage = (Resolve-Path -LiteralPath $stage).Path
    if ((Split-Path $resolvedStage) -eq $temp -and (Split-Path $resolvedStage -Leaf) -like 'RepriseMsix-*') {
        Remove-Item -LiteralPath $resolvedStage -Recurse -Force
    }
}
