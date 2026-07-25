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

function Get-PeRawSections {
    param(
        [Parameter(Mandatory)] [string]$Path,
        [Parameter(Mandatory)] [string]$Label
    )

    $sections = @()
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
        $stream.Position = $peOffset + 6
        $sectionCount = $reader.ReadUInt16()
        $stream.Position = $peOffset + 20
        $optionalHeaderSize = $reader.ReadUInt16()
        $optionalHeaderOffset = $peOffset + 24
        $stream.Position = $optionalHeaderOffset
        if ($reader.ReadUInt16() -ne 0x020B) {
            throw "$Label is not PE32+ (x64)."
        }
        if ($sectionCount -lt 1 -or $sectionCount -gt 96 -or
                $optionalHeaderSize -lt 64) {
            throw "$Label has invalid PE section metadata."
        }
        $sectionTable = $optionalHeaderOffset + $optionalHeaderSize
        if ($sectionTable + 40 * $sectionCount -gt $stream.Length) {
            throw "$Label section table exceeds the file."
        }
        for ($index = 0; $index -lt $sectionCount; ++$index) {
            $sectionOffset = $sectionTable + 40 * $index
            $stream.Position = $sectionOffset
            $nameBytes = $reader.ReadBytes(8)
            $name = ([Text.Encoding]::ASCII.GetString($nameBytes)).Trim([char]0)
            $virtualSize = $reader.ReadUInt32()
            $virtualAddress = $reader.ReadUInt32()
            $rawSize = $reader.ReadUInt32()
            $rawOffset = $reader.ReadUInt32()
            $stream.Position = $sectionOffset + 36
            $characteristics = $reader.ReadUInt32()
            if ([uint64]$rawOffset + [uint64]$rawSize -gt
                    [uint64]$stream.Length) {
                throw "$Label section $name exceeds the file."
            }
            $span = [Math]::Max([uint64]$virtualSize, [uint64]$rawSize)
            $sections += [pscustomobject]@{
                Name = $name
                StartRva = [uint64]$virtualAddress
                EndRva = [uint64]$virtualAddress + $span
                RawOffset = [uint64]$rawOffset
                RawSize = [uint64]$rawSize
                Executable = ($characteristics -band 0x20000000) -ne 0
            }
        }
    }
    finally {
        $stream.Dispose()
    }
    return $sections
}

function Convert-AobTokens {
    param(
        [Parameter(Mandatory)] [string]$Pattern,
        [Parameter(Mandatory)] [string]$Label
    )

    $tokens = @()
    foreach ($token in ($Pattern -split '\s+' | Where-Object { $_ })) {
        if ($token -eq '??') {
            $tokens += -1
        }
        elseif ($token -match '^[0-9A-Fa-f]{2}$') {
            $tokens += [Convert]::ToInt32($token, 16)
        }
        else {
            throw "Invalid AOB token for $Label`: $token"
        }
    }
    if ($tokens.Count -eq 0) {
        throw "Empty AOB for $Label."
    }
    return $tokens
}

function Find-ExecutableAobMatches {
    param(
        [Parameter(Mandatory)] [byte[]]$Bytes,
        [Parameter(Mandatory)] [object[]]$Sections,
        [Parameter(Mandatory)] [int[]]$Pattern
    )

    $matches = @()
    foreach ($section in $Sections) {
        if (-not $section.Executable -or $section.RawSize -lt $Pattern.Count) {
            continue
        }
        $first = [int64]$section.RawOffset
        $lastExclusive = [Math]::Min(
            [int64]$Bytes.LongLength,
            [int64]$section.RawOffset + [int64]$section.RawSize)
        $lastStart = $lastExclusive - $Pattern.Count
        for ($offset = $first; $offset -le $lastStart; ++$offset) {
            if ($Pattern[0] -ge 0 -and $Bytes[$offset] -ne $Pattern[0]) {
                continue
            }
            $matched = $true
            for ($index = 1; $index -lt $Pattern.Count; ++$index) {
                if ($Pattern[$index] -ge 0 -and
                        $Bytes[$offset + $index] -ne $Pattern[$index]) {
                    $matched = $false
                    break
                }
            }
            if ($matched) {
                $matches += [uint64]$section.StartRva +
                    ([uint64]$offset - [uint64]$section.RawOffset)
            }
        }
    }
    return $matches
}

function Test-FunctionEvidence {
    param(
        [Parameter(Mandatory)] [byte[]]$Bytes,
        [Parameter(Mandatory)] [object[]]$Sections,
        [Parameter(Mandatory)] [object]$Evidence,
        [Parameter(Mandatory)] [string]$Label
    )

    $rva = [uint64](Convert-HexUInt32 ([string]$Evidence.function_rva) "$Label RVA")
    $end = [uint64](Convert-HexUInt32 `
        ([string]$Evidence.function_end_rva_exclusive) "$Label end RVA")
    $size = [uint64]$Evidence.function_size
    if ($end -le $rva -or $end - $rva -ne $size) {
        throw "$Label function boundary/size is inconsistent."
    }
    $pattern = [int[]]@(Convert-AobTokens ([string]$Evidence.entry_aob) $Label)
    $matches = @(Find-ExecutableAobMatches $Bytes $Sections $pattern)
    $expectedCount = [int]$Evidence.static_executable_match_count
    if ($matches.Count -ne $expectedCount -or $expectedCount -ne 1 -or
            $matches[0] -ne $rva) {
        $rendered = ($matches | ForEach-Object { '0x{0:X8}' -f $_ }) -join ', '
        throw "$Label AOB mismatch: expected one at 0x$('{0:X8}' -f $rva), got [$rendered]."
    }
    $owners = @($Sections | Where-Object {
        $_.Executable -and $rva -ge $_.StartRva -and
            $rva + $size -le $_.StartRva + $_.RawSize
    })
    if ($owners.Count -ne 1) {
        throw "$Label body is not contained in exactly one raw executable PE section."
    }
    $owner = $owners[0]
    $range = [pscustomobject]@{
        Offset = [uint64]$owner.RawOffset + ($rva - $owner.StartRva)
        Size = $size
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha.ComputeHash($Bytes, [int]$range.Offset, [int]$range.Size)
    }
    finally {
        $sha.Dispose()
    }
    $actualBodyHash = ([BitConverter]::ToString($digest)).Replace('-', '')
    if ($actualBodyHash -ine [string]$Evidence.body_sha256) {
        throw "$Label body SHA-256 mismatch: $actualBodyHash"
    }
    Write-Host ("Function {0}: exact AOB/body at RVA 0x{1:X8}" -f $Label, $rva)
}

function Test-FunctionBodyEvidence {
    param(
        [Parameter(Mandatory)] [string]$Path,
        [Parameter(Mandatory)] [object[]]$Sections,
        [Parameter(Mandatory)] [object]$Evidence,
        [Parameter(Mandatory)] [string]$Label
    )

    $rva = [uint64](Convert-HexUInt32 ([string]$Evidence.player_view_render_rva) `
        "$Label RVA")
    $end = [uint64](Convert-HexUInt32 `
        ([string]$Evidence.player_view_render_end_rva_exclusive) "$Label end RVA")
    $size = [uint64]$Evidence.player_view_render_size
    if ($end -le $rva -or $end - $rva -ne $size -or
            $size -gt [int]::MaxValue) {
        throw "$Label function boundary/size is inconsistent."
    }
    $owners = @($Sections | Where-Object {
        $_.Executable -and $rva -ge $_.StartRva -and
            $rva + $size -le $_.StartRva + $_.RawSize
    })
    if ($owners.Count -ne 1) {
        throw "$Label body is not contained in exactly one raw executable PE section."
    }
    $owner = $owners[0]
    $range = [pscustomobject]@{
        Offset = [uint64]$owner.RawOffset + ($rva - $owner.StartRva)
        Size = $size
    }
    $body = [byte[]]::new([int]$range.Size)
    $stream = [IO.File]::OpenRead($Path)
    try {
        $stream.Position = [int64]$range.Offset
        $read = 0
        while ($read -lt $body.Length) {
            $count = $stream.Read($body, $read, $body.Length - $read)
            if ($count -le 0) {
                throw "$Label body read was truncated."
            }
            $read += $count
        }
    }
    finally {
        $stream.Dispose()
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha.ComputeHash($body)
    }
    finally {
        $sha.Dispose()
    }
    $actualBodyHash = ([BitConverter]::ToString($digest)).Replace('-', '')
    if ($actualBodyHash -ine [string]$Evidence.player_view_render_body_sha256) {
        throw "$Label body SHA-256 mismatch: $actualBodyHash"
    }
    Write-Host ("Function {0}: exact body at RVA 0x{1:X8}" -f $Label, $rva)
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

$hrekHomolog = $manifest.player_view_transaction.hrek_homolog
if ([IO.Path]::GetFileName([string]$hrekHomolog.binary) -cne
        [string]$cameraEvidence.name) {
    throw 'HREK homolog binary does not match the pinned camera evidence binary.'
}
$hrekSections = @(Get-PeRawSections `
    -Path $cameraEvidencePath -Label 'HREK camera evidence binary')
Test-FunctionBodyEvidence -Path $cameraEvidencePath -Sections $hrekSections `
    -Evidence $hrekHomolog -Label 'HREK player_view_render'

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
        $rawOffset = $reader.ReadUInt32()
        $stream.Position = $sectionOffset + 36
        $characteristics = $reader.ReadUInt32()
        $span = [Math]::Max([uint64]$virtualSize, [uint64]$rawSize)
        $sections += [pscustomobject]@{
            Name = $name
            StartRva = [uint64]$virtualAddress
            EndRva = [uint64]$virtualAddress + $span
            RawOffset = [uint64]$rawOffset
            RawSize = [uint64]$rawSize
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

if (@($manifest.preliminary_candidates).Count -ne 5) {
    throw 'Reach evidence manifest must contain the five recorded candidates.'
}
foreach ($candidate in $manifest.preliminary_candidates) {
    $productionFp = $candidate.id -eq 'fp_interpolation' -or
        $candidate.id -eq 'visible_palette' -or
        $candidate.id -eq 'first_person_camera_upload'
    if ($candidate.proof_complete -ne $productionFp) {
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
    $proofLabel = if ($productionFp) { 'production proof recorded; headset pending' }
        else { 'runtime proof incomplete' }
    Write-Host ('Candidate {0}: RVA 0x{1:X8}, offline section {2}, {3}' -f
        $candidate.id, $candidateRva, $executableOwners[0].Name, $proofLabel)
}

$moduleBytes = [IO.File]::ReadAllBytes($ModulePath)
$nativeWeaponIk = $manifest.native_weapon_ik_bypass
if ($null -eq $nativeWeaponIk -or
        $nativeWeaponIk.static_proof_complete -ne $true -or
        $nativeWeaponIk.headset_accepted -ne $false -or
        [string]$nativeWeaponIk.retail.control_name -cne
            'debug_animation_fp_weapon_ik_disable' -or
        [string]$nativeWeaponIk.hrek.control_name -cne
            'debug_animation_fp_weapon_ik_disable' -or
        [int]$nativeWeaponIk.retail.control_type -ne 5 -or
        [int]$nativeWeaponIk.hrek.control_type -ne 5 -or
        $nativeWeaponIk.retail.descriptor_value_pointer_published -ne $false -or
        $nativeWeaponIk.hrek.descriptor_value_pointer_published -ne $true -or
        [string]$nativeWeaponIk.hrek.binary -cne
            [string]$cameraEvidence.name) {
    throw 'Reach native weapon-IK evidence identity is incomplete or inconsistent.'
}
$hrekBytes = [IO.File]::ReadAllBytes($cameraEvidencePath)
$weaponIkImages = @(
    [pscustomobject]@{
        Label = 'retail native weapon-IK decision'
        Bytes = $moduleBytes
        Sections = $sections
        Evidence = $nativeWeaponIk.retail
    },
    [pscustomobject]@{
        Label = 'HREK native weapon-IK decision'
        Bytes = $hrekBytes
        Sections = $hrekSections
        Evidence = $nativeWeaponIk.hrek
    }
)
foreach ($image in $weaponIkImages) {
    $pattern = [int[]]@(Convert-AobTokens `
        ([string]$image.Evidence.decision_aob) $image.Label)
    $matches = @(Find-ExecutableAobMatches `
        $image.Bytes $image.Sections $pattern)
    $expectedRva = [uint64](Convert-HexUInt32 `
        ([string]$image.Evidence.decision_aob_rva) "$($image.Label) RVA")
    $expectedCount = [int]$image.Evidence.static_executable_match_count
    if ($expectedCount -ne 1 -or $matches.Count -ne 1 -or
            $matches[0] -ne $expectedRva) {
        $rendered = ($matches | ForEach-Object {
            '0x{0:X8}' -f $_
        }) -join ', '
        throw "$($image.Label) AOB mismatch: expected one at 0x$('{0:X8}' -f $expectedRva), got [$rendered]."
    }
    Write-Host ('Function {0}: exact {1}-byte AOB at RVA 0x{2:X8}' -f `
        $image.Label, $pattern.Count, $expectedRva)
}
$frustumEvidence = @($manifest.preliminary_candidates | Where-Object {
    $_.id -eq 'viewport'
})
if ($frustumEvidence.Count -ne 1) {
    throw 'Reach evidence manifest must contain exactly one viewport/frustum candidate.'
}
$frustumPattern = [int[]]@(Convert-AobTokens `
    ([string]$frustumEvidence[0].aob) 'frustum helper')
if ($frustumPattern.Count -ne 25) {
    throw "Canonical Reach frustum AOB must contain exactly 25 bytes; got $($frustumPattern.Count)."
}
$frustumMatches = @(Find-ExecutableAobMatches `
    $moduleBytes $sections $frustumPattern)
$frustumRva = [uint64](Convert-HexUInt32 `
    ([string]$frustumEvidence[0].rva) 'frustum helper RVA')
if ($frustumMatches.Count -ne 1 -or $frustumMatches[0] -ne $frustumRva) {
    $rendered = ($frustumMatches | ForEach-Object {
        '0x{0:X8}' -f $_
    }) -join ', '
    throw "Frustum helper AOB mismatch: expected one 25-byte match at 0x$('{0:X8}' -f $frustumRva), got [$rendered]."
}
Write-Host ('Function frustum helper: exact 25-byte AOB at RVA 0x{0:X8}' -f `
    $frustumRva)
foreach ($fpId in @('fp_interpolation', 'visible_palette')) {
    $evidence = @($manifest.preliminary_candidates | Where-Object {
        $_.id -eq $fpId
    })
    if ($evidence.Count -ne 1) {
        throw "Reach evidence manifest must contain exactly one $fpId candidate."
    }
    $pattern = [int[]]@(Convert-AobTokens ([string]$evidence[0].aob) $fpId)
    $matches = @(Find-ExecutableAobMatches $moduleBytes $sections $pattern)
    $expectedRva = [uint64](Convert-HexUInt32 `
        ([string]$evidence[0].rva) "$fpId RVA")
    if ($matches.Count -ne 1 -or $matches[0] -ne $expectedRva) {
        $rendered = ($matches | ForEach-Object { '0x{0:X8}' -f $_ }) -join ', '
        throw "$fpId AOB mismatch: expected one match at 0x$('{0:X8}' -f $expectedRva), got [$rendered]."
    }
    Write-Host ('Function {0}: exact {1}-byte AOB at RVA 0x{2:X8}' -f `
        $fpId, $pattern.Count, $expectedRva)
}
$fpCameraEvidence = @($manifest.preliminary_candidates | Where-Object {
    $_.id -eq 'first_person_camera_upload'
})
if ($fpCameraEvidence.Count -ne 1 -or
        $fpCameraEvidence[0].headset_accepted -ne $false -or
        [string]$fpCameraEvidence[0].hrek_homolog.binary -cne
            [string]$cameraEvidence.name) {
    throw 'Reach evidence manifest must contain one statically proven, headset-pending FP camera transaction.'
}
Test-FunctionEvidence -Bytes $moduleBytes -Sections $sections `
    -Evidence $fpCameraEvidence[0].rebuild `
    -Label 'retail first-person camera rebuild'
Test-FunctionEvidence -Bytes $moduleBytes -Sections $sections `
    -Evidence $fpCameraEvidence[0].uploader `
    -Label 'retail first-person camera uploader'
Test-FunctionEvidence -Bytes $hrekBytes -Sections $hrekSections `
    -Evidence $fpCameraEvidence[0].hrek_homolog.rebuild `
    -Label 'HREK first-person camera rebuild'
Test-FunctionEvidence -Bytes $hrekBytes -Sections $hrekSections `
    -Evidence $fpCameraEvidence[0].hrek_homolog.uploader `
    -Label 'HREK first-person camera uploader'
Test-FunctionEvidence -Bytes $moduleBytes -Sections $sections `
    -Evidence $manifest.player_view_transaction.retail.main_render_view `
    -Label 'main_render_view'
Test-FunctionEvidence -Bytes $moduleBytes -Sections $sections `
    -Evidence $manifest.player_view_transaction.retail.player_view_render `
    -Label 'player_view_render'

Write-Host 'Reach evidence preflight passed.'
Write-Host "Module SHA-256: $actualHash"
Write-Host "HREK build:       $actualHrekBuild"
Write-Host "HREK camera SHA:  $actualCameraEvidenceHash"
Write-Host 'Helper hooks:     forbidden (top-level hook_eligible=false)'
Write-Host 'Camera runtime:   permanent transaction audited separately; headset acceptance pending'
