[CmdletBinding()]
param(
    [string]$ModulePath =
        'N:\SteamLibrary\steamapps\common\Halo The Master Chief Collection\haloreach\haloreach.dll',
    [string]$HrekPath = 'N:\SteamLibrary\steamapps\common\HREK',
    [string]$ManifestPath
)

$ErrorActionPreference = 'Stop'

if (-not $ManifestPath) {
    $ManifestPath = Join-Path $PSScriptRoot '..\docs\REACH-EVIDENCE-MANIFEST.json'
}

function Convert-HexUInt32 {
    param(
        [Parameter(Mandatory)] [string]$Value,
        [Parameter(Mandatory)] [string]$Field
    )

    if ($Value -notmatch '^0x[0-9A-Fa-f]{1,8}$') {
        throw "Invalid $Field value in Reach evidence manifest: $Value"
    }
    return [Convert]::ToUInt32($Value.Substring(2), 16)
}

function Get-PeIdentity {
    param(
        [Parameter(Mandatory)] [string]$Path,
        [Parameter(Mandatory)] [string]$Label
    )

    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) {
            throw "$Label is not an MZ executable."
        }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0x40 -or $peOffset -gt $stream.Length - 256) {
            throw "$Label has an invalid PE header offset."
        }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "$Label has no PE signature."
        }
        $machine = $reader.ReadUInt16()
        $stream.Position = $peOffset + 8
        $timestamp = $reader.ReadUInt32()
        $optionalHeaderOffset = $peOffset + 24
        $stream.Position = $optionalHeaderOffset
        if ($reader.ReadUInt16() -ne 0x020B) {
            throw "$Label is not PE32+ (x64)."
        }
        $stream.Position = $optionalHeaderOffset + 56
        $sizeOfImage = $reader.ReadUInt32()
        return [pscustomobject]@{
            Machine = $machine
            Timestamp = $timestamp
            SizeOfImage = $sizeOfImage
        }
    }
    finally {
        $stream.Dispose()
    }
}

foreach ($requiredFile in @($ModulePath, $ManifestPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required Reach evidence file is missing: $requiredFile"
    }
}
if (-not (Test-Path -LiteralPath $HrekPath -PathType Container)) {
    throw "Required HREK directory is missing: $HrekPath"
}

$manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
if ($manifest.hook_eligible -ne $false) {
    throw 'Reach evidence manifest must remain hook_eligible=false.'
}
if ([IO.Path]::GetFileName($ModulePath) -ine $manifest.retail_module.name) {
    throw 'Reach module filename does not match the evidence manifest.'
}

$actualHash = (Get-FileHash -LiteralPath $ModulePath -Algorithm SHA256).Hash
if ($actualHash -ine $manifest.retail_module.sha256) {
    throw "Reach module SHA-256 mismatch: $actualHash"
}

$buildTagPath = Join-Path $HrekPath 'build_tag.txt'
if (-not (Test-Path -LiteralPath $buildTagPath -PathType Leaf)) {
    throw "HREK build tag is missing: $buildTagPath"
}
$actualHrekBuild = (Get-Content -LiteralPath $buildTagPath -TotalCount 1).Trim()
if ($actualHrekBuild -cne $manifest.editing_kit.build) {
    throw "HREK build mismatch: $actualHrekBuild"
}
foreach ($requiredRoot in $manifest.editing_kit.required_roots) {
    $rootPath = Join-Path $HrekPath $requiredRoot
    if (-not (Test-Path -LiteralPath $rootPath -PathType Container)) {
        throw "Required extracted HREK root is missing: $rootPath"
    }
}

$cameraEvidence = $manifest.editing_kit.camera_evidence_binary
if ($null -eq $cameraEvidence -or
        [string]::IsNullOrWhiteSpace([string]$cameraEvidence.name) -or
        [IO.Path]::GetFileName([string]$cameraEvidence.name) -cne
            [string]$cameraEvidence.name) {
    throw 'HREK camera evidence binary identity is missing or invalid.'
}
$cameraEvidencePath = Join-Path $HrekPath ([string]$cameraEvidence.name)
if (-not (Test-Path -LiteralPath $cameraEvidencePath -PathType Leaf)) {
    throw "HREK camera evidence binary is missing: $cameraEvidencePath"
}
$actualCameraEvidenceHash =
    (Get-FileHash -LiteralPath $cameraEvidencePath -Algorithm SHA256).Hash
if ($actualCameraEvidenceHash -ine [string]$cameraEvidence.sha256) {
    throw "HREK camera evidence binary SHA-256 mismatch: $actualCameraEvidenceHash"
}
$expectedCameraEvidenceTimestamp = Convert-HexUInt32 `
    ([string]$cameraEvidence.pe_timestamp) 'HREK camera evidence PE timestamp'
$expectedCameraEvidenceImageSize = Convert-HexUInt32 `
    ([string]$cameraEvidence.size_of_image) 'HREK camera evidence SizeOfImage'
$cameraEvidenceIdentity = Get-PeIdentity `
    -Path $cameraEvidencePath -Label 'HREK camera evidence binary'
if ($cameraEvidenceIdentity.Machine -ne 0x8664 -or
        $cameraEvidenceIdentity.Timestamp -ne
            $expectedCameraEvidenceTimestamp -or
        $cameraEvidenceIdentity.SizeOfImage -ne
            $expectedCameraEvidenceImageSize) {
    throw ('HREK camera evidence PE identity mismatch: machine=0x{0:X4}, ' +
        'timestamp=0x{1:X8}, SizeOfImage=0x{2:X8}' -f
        $cameraEvidenceIdentity.Machine,
        $cameraEvidenceIdentity.Timestamp,
        $cameraEvidenceIdentity.SizeOfImage)
}

$expectedTimestamp = Convert-HexUInt32 `
    $manifest.retail_module.pe_timestamp 'PE timestamp'
$expectedImageSize = Convert-HexUInt32 `
    $manifest.retail_module.size_of_image 'SizeOfImage'

$sections = @()
$stream = [IO.File]::OpenRead($ModulePath)
try {
    $reader = [IO.BinaryReader]::new($stream)
    if ($reader.ReadUInt16() -ne 0x5A4D) {
        throw 'Reach module is not an MZ executable.'
    }
    $stream.Position = 0x3C
    $peOffset = $reader.ReadInt32()
    if ($peOffset -lt 0x40 -or $peOffset -gt $stream.Length - 256) {
        throw 'Reach module has an invalid PE header offset.'
    }
    $stream.Position = $peOffset
    if ($reader.ReadUInt32() -ne 0x00004550) {
        throw 'Reach module has no PE signature.'
    }
    $stream.Position = $peOffset + 6
    $sectionCount = $reader.ReadUInt16()
    $stream.Position = $peOffset + 8
    $timestamp = $reader.ReadUInt32()
    $stream.Position = $peOffset + 20
    $optionalHeaderSize = $reader.ReadUInt16()
    $optionalHeaderOffset = $peOffset + 24
    $stream.Position = $optionalHeaderOffset
    if ($reader.ReadUInt16() -ne 0x020B) {
        throw 'Reach module is not PE32+ (x64).'
    }
    $stream.Position = $optionalHeaderOffset + 56
    $sizeOfImage = $reader.ReadUInt32()
    if ($sectionCount -lt 1 -or $sectionCount -gt 96 -or
            $optionalHeaderSize -lt 64) {
        throw 'Reach module has invalid PE section metadata.'
    }
    $sectionTable = $optionalHeaderOffset + $optionalHeaderSize
    if ($sectionTable + 40 * $sectionCount -gt $stream.Length) {
        throw 'Reach module section table exceeds the file.'
    }
    for ($index = 0; $index -lt $sectionCount; ++$index) {
        $sectionOffset = $sectionTable + 40 * $index
        $stream.Position = $sectionOffset
        $nameBytes = $reader.ReadBytes(8)
        $name = ([Text.Encoding]::ASCII.GetString($nameBytes)).Trim([char]0)
        $virtualSize = $reader.ReadUInt32()
        $virtualAddress = $reader.ReadUInt32()
        $rawSize = $reader.ReadUInt32()
        $stream.Position = $sectionOffset + 36
        $characteristics = $reader.ReadUInt32()
        $span = [Math]::Max([uint64]$virtualSize, [uint64]$rawSize)
        $sections += [pscustomobject]@{
            Name = $name
            StartRva = [uint64]$virtualAddress
            EndRva = [uint64]$virtualAddress + $span
            Executable = ($characteristics -band 0x20000000) -ne 0
        }
    }
}
finally {
    $stream.Dispose()
}

if ($timestamp -ne $expectedTimestamp) {
    throw ('Reach PE timestamp mismatch: 0x{0:X8}' -f $timestamp)
}
if ($sizeOfImage -ne $expectedImageSize) {
    throw ('Reach SizeOfImage mismatch: 0x{0:X8}' -f $sizeOfImage)
}

if (@($manifest.preliminary_candidates).Count -ne 4) {
    throw 'Reach evidence manifest must contain the four preliminary candidates.'
}
foreach ($candidate in $manifest.preliminary_candidates) {
    if ($candidate.proof_complete -ne $false) {
        throw "Preliminary Reach candidate is incorrectly proof-complete: $($candidate.id)"
    }
    $candidateRva = [uint64](Convert-HexUInt32 $candidate.rva `
        "candidate $($candidate.id) RVA")
    $executableOwners = @($sections | Where-Object {
        $_.Executable -and $candidateRva -ge $_.StartRva -and
            $candidateRva -lt $_.EndRva
    })
    if ($executableOwners.Count -ne 1) {
        throw "Candidate $($candidate.id) is not in exactly one executable PE section."
    }
    Write-Host ('Candidate {0}: RVA 0x{1:X8}, offline section {2}, runtime proof incomplete' -f
        $candidate.id, $candidateRva, $executableOwners[0].Name)
}

Write-Host 'Reach evidence preflight passed.'
Write-Host "Module SHA-256: $actualHash"
Write-Host "HREK build:       $actualHrekBuild"
Write-Host "HREK camera SHA:  $actualCameraEvidenceHash"
Write-Host 'Runtime hooks:    forbidden (hook_eligible=false)'
