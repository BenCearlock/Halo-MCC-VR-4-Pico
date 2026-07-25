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

function Get-ExecutableBytesAtRva {
    param(
        [Parameter(Mandatory)] [byte[]]$Bytes,
        [Parameter(Mandatory)] [object[]]$Sections,
        [Parameter(Mandatory)] [uint64]$Rva,
        [Parameter(Mandatory)] [uint64]$Size,
        [Parameter(Mandatory)] [string]$Label
    )

    if ($Size -eq 0 -or $Size -gt [int]::MaxValue) {
        throw "$Label has an invalid byte-range size."
    }
    $owners = @($Sections | Where-Object {
        $_.Executable -and $Rva -ge $_.StartRva -and
            $Rva + $Size -le $_.StartRva + $_.RawSize
    })
    if ($owners.Count -ne 1) {
        throw "$Label is not contained in exactly one raw executable PE section."
    }
    $owner = $owners[0]
    $rawOffset = [uint64]$owner.RawOffset + ($Rva - $owner.StartRva)
    if ($rawOffset + $Size -gt [uint64]$Bytes.LongLength -or
            $rawOffset -gt [int]::MaxValue) {
        throw "$Label exceeds the pinned image bytes."
    }
    $result = [byte[]]::new([int]$Size)
    [Buffer]::BlockCopy($Bytes, [int]$rawOffset, $result, 0, [int]$Size)
    return ,$result
}

function Get-ByteSha256 {
    param([Parameter(Mandatory)] [byte[]]$Bytes)

    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha.ComputeHash($Bytes)
    }
    finally {
        $sha.Dispose()
    }
    return ([BitConverter]::ToString($digest)).Replace('-', '')
}

function Test-ExecutableRangeEvidence {
    param(
        [Parameter(Mandatory)] [byte[]]$Bytes,
        [Parameter(Mandatory)] [object[]]$Sections,
        [Parameter(Mandatory)] [object]$Evidence,
        [Parameter(Mandatory)] [string]$Label
    )

    $rva = [uint64](Convert-HexUInt32 `
        ([string]$Evidence.containing_block_rva) "$Label RVA")
    $end = [uint64](Convert-HexUInt32 `
        ([string]$Evidence.containing_block_end_rva_exclusive) "$Label end RVA")
    $size = [uint64]$Evidence.containing_block_size
    if ($end -le $rva -or $end - $rva -ne $size) {
        throw "$Label boundary/size is inconsistent."
    }
    $pattern = [int[]]@(Convert-AobTokens `
        ([string]$Evidence.containing_block_aob) $Label)
    if ($pattern.Count -ne $size) {
        throw "$Label AOB must cover its complete $size-byte range."
    }
    $matches = @(Find-ExecutableAobMatches $Bytes $Sections $pattern)
    $expectedCount = [int]$Evidence.static_executable_match_count
    if ($expectedCount -ne 1 -or $matches.Count -ne 1 -or
            $matches[0] -ne $rva) {
        $rendered = ($matches | ForEach-Object {
            '0x{0:X8}' -f $_
        }) -join ', '
        throw "$Label AOB mismatch: expected one at 0x$('{0:X8}' -f $rva), got [$rendered]."
    }
    $rangeBytes = Get-ExecutableBytesAtRva `
        -Bytes $Bytes -Sections $Sections -Rva $rva -Size $size -Label $Label
    $actualHash = Get-ByteSha256 -Bytes $rangeBytes
    if ($actualHash -ine [string]$Evidence.containing_block_sha256) {
        throw "$Label SHA-256 mismatch: $actualHash"
    }
    Write-Host ("Range {0}: exact AOB/body at RVA 0x{1:X8}" -f $Label, $rva)
}

function Test-ExactExecutableBytes {
    param(
        [Parameter(Mandatory)] [byte[]]$Bytes,
        [Parameter(Mandatory)] [object[]]$Sections,
        [Parameter(Mandatory)] [uint64]$Rva,
        [Parameter(Mandatory)] [string]$ExpectedBytes,
        [Parameter(Mandatory)] [string]$ExpectedSha256,
        [Parameter(Mandatory)] [string]$Label
    )

    $tokens = [int[]]@(Convert-AobTokens $ExpectedBytes $Label)
    if (@($tokens | Where-Object { $_ -lt 0 }).Count -ne 0) {
        throw "$Label exact bytes must not contain wildcards."
    }
    $actual = Get-ExecutableBytesAtRva -Bytes $Bytes -Sections $Sections `
        -Rva $Rva -Size ([uint64]$tokens.Count) -Label $Label
    for ($index = 0; $index -lt $tokens.Count; ++$index) {
        if ($actual[$index] -ne $tokens[$index]) {
            throw "$Label byte mismatch at +0x$('{0:X}' -f $index)."
        }
    }
    $actualHash = Get-ByteSha256 -Bytes $actual
    if ($actualHash -ine $ExpectedSha256) {
        throw "$Label SHA-256 mismatch: $actualHash"
    }
    Write-Host ("Bytes {0}: exact {1}-byte body at RVA 0x{2:X8}" -f `
        $Label, $tokens.Count, $Rva)
}

function Test-Rel32CallEvidence {
    param(
        [Parameter(Mandatory)] [byte[]]$Bytes,
        [Parameter(Mandatory)] [object[]]$Sections,
        [Parameter(Mandatory)] [uint64]$CallSiteRva,
        [Parameter(Mandatory)] [uint64]$TargetRva,
        [Parameter(Mandatory)] [string]$Label
    )

    $instruction = Get-ExecutableBytesAtRva -Bytes $Bytes -Sections $Sections `
        -Rva $CallSiteRva -Size 5 -Label $Label
    if ($instruction[0] -ne 0xE8) {
        throw "$Label is not a direct rel32 call."
    }
    $displacement = [BitConverter]::ToInt32($instruction, 1)
    $actualTarget = [int64]$CallSiteRva + 5 + [int64]$displacement
    if ($actualTarget -ne [int64]$TargetRva) {
        throw ('{0} target mismatch: expected 0x{1:X8}, got 0x{2:X8}.' -f `
            $Label, $TargetRva, $actualTarget)
    }
    Write-Host ('Edge {0}: 0x{1:X8} -> 0x{2:X8}' -f `
        $Label, $CallSiteRva, $TargetRva)
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
$nativeProjectile = $manifest.native_projectile_origin
if ($null -eq $nativeProjectile -or
        $nativeProjectile.static_proof_complete -ne $true -or
        $nativeProjectile.headset_accepted -ne $false -or
        $nativeProjectile.accepted_pointer_advanced -ne $false) {
    throw 'Reach native projectile-origin evidence must remain statically proven and headset-pending.'
}
$projectileRetail = $nativeProjectile.retail
$projectileFire = $projectileRetail.projectile_fire
$projectileDecision = $projectileRetail.origin_decision
$projectileSlot = $projectileRetail.output_user_weapon_slot
$projectileHrek = $nativeProjectile.hrek
$projectileScope = $nativeProjectile.runtime_scope
if ([string]$projectileFire.function_rva -cne '0x004C2710' -or
        [string]$projectileFire.function_end_rva_exclusive -cne '0x004C4923' -or
        [int]$projectileFire.function_size -ne 8723 -or
        [string]$projectileFire.abi -cne
            'void __fastcall(int weapon_datum_index,int16 barrel_index,void* firing_context,bool simulation_or_prediction)' -or
        [int]$projectileFire.direct_rel32_caller_count -ne 1 -or
        [string]$projectileFire.direct_rel32_call_site_rva -cne '0x004B63BE' -or
        [string]$projectileFire.wrapper_rva -cne '0x004B6318' -or
        [string]$projectileFire.wrapper_end_rva_exclusive -cne '0x004B640A' -or
        (@($projectileFire.wrapper_upstream_rel32_call_sites) -join ',') -cne
            '0x00435EB6,0x004B62FE') {
    throw 'Reach retail projectile-fire function/caller identity is inconsistent.'
}
if ([string]$projectileDecision.containing_block_rva -cne '0x004C30AC' -or
        [string]$projectileDecision.containing_block_end_rva_exclusive -cne
            '0x004C30F4' -or
        [int]$projectileDecision.containing_block_size -ne 72 -or
        [string]$projectileDecision.decision_rva -cne '0x004C30C5' -or
        [int]$projectileDecision.decision_size -ne 5 -or
        [string]$projectileDecision.decision_bytes -cne '41 22 C5 75 0A' -or
        [string]$projectileDecision.stock_false_continuation_rva -cne
            '0x004C30CA' -or
        [string]$projectileDecision.weapon_origin_continuation_rva -cne
            '0x004C30D4' -or
        [string]$projectileDecision.marker_origin_copy_start_rva -cne
            '0x004C30D8' -or
        [string]$projectileDecision.marker_origin_copy_end_rva_exclusive -cne
            '0x004C30F4' -or
        $projectileDecision.projectile_direction_unchanged -ne $true) {
    throw 'Reach retail projectile origin-decision identity is inconsistent.'
}
if ([string]$projectileSlot.function_rva -cne '0x002B1218' -or
        [string]$projectileSlot.function_end_rva_exclusive -cne '0x002B1273' -or
        [int]$projectileSlot.function_size -ne 91 -or
        [string]$projectileSlot.abi -cne
            'int __fastcall(int output_user_index,int weapon_datum_index)' -or
        [string]$projectileSlot.first_person_consumer_function_rva -cne
            '0x00120FDC' -or
        [string]$projectileSlot.first_person_consumer_function_end_rva_exclusive -cne
            '0x001210D3' -or
        [string]$projectileSlot.first_person_consumer_rel32_call_site_rva -cne
            '0x0012101A' -or
        [string]$projectileSlot.output_user_stride -cne '0x000053A8' -or
        [int]$projectileSlot.slot_count -ne 2 -or
        [string]$projectileSlot.slot_stride -cne '0x00002978' -or
        [string]$projectileSlot.weapon_datum_offset -cne '0x0000003C' -or
        [int]$projectileSlot.primary_slot_index -ne 0 -or
        [int]$projectileSlot.not_found_result -ne -1) {
    throw 'Reach retail output-user first-person weapon-slot identity is inconsistent.'
}
if ([string]$projectileHrek.binary -cne [string]$cameraEvidence.name -or
        [string]$projectileHrek.projectile_fire_function_rva -cne '0x00DE4290' -or
        [string]$projectileHrek.projectile_fire_function_end_rva_exclusive -cne
            '0x00DE8198' -or
        [string]$projectileHrek.origin_consumer_start_rva -cne '0x00DE5289' -or
        [string]$projectileHrek.origin_consumer_end_rva_exclusive -cne
            '0x00DE52CA' -or
        [string]$projectileHrek.projectiles_use_weapon_origin_name -cne
            'projectiles use weapon origin' -or
        [int]$projectileHrek.projectiles_use_weapon_origin_bit -ne 2 -or
        [string]$projectileHrek.projectiles_use_weapon_origin_mask -cne
            '0x00000004' -or
        [string]$projectileHrek.projectile_fires_in_marker_direction_name -cne
            'projectile fires in marker direction' -or
        [int]$projectileHrek.projectile_fires_in_marker_direction_bit -ne 15 -or
        [string]$projectileHrek.projectile_fires_in_marker_direction_mask -cne
            '0x00008000' -or
        $projectileHrek.projectile_fires_in_marker_direction_excluded -ne $true -or
        [string]$projectileHrek.first_person_weapon_validate_call_site_rva -cne
            '0x003723EA' -or
        [string]$projectileHrek.first_person_weapon_validate_rva -cne
            '0x008D2670' -or
        [string]$projectileHrek.first_person_weapon_slot_for_datum_rva -cne
            '0x008CEED0' -or
        [string]$projectileHrek.official_assault_rifle_barrel_flags -cne 'empty' -or
        [string]$projectileHrek.official_assault_rifle_marker -cne
            'primary_trigger' -or
        [string]$projectileHrek.official_assault_rifle_marker_centered_and_forward_axis -cne
            '+X') {
    throw 'Reach HREK projectile-origin semantics are incomplete or inconsistent.'
}
if ([int]$projectileScope.output_user_index -ne 0 -or
        [int]$projectileScope.required_slot_index -ne 0 -or
        $projectileScope.incoming_full_weapon_datum_must_match -ne $true -or
        $projectileScope.ai_remote_third_person_and_vehicle_weapons_excluded_by_exact_slot_match -ne
            $true -or
        $projectileScope.shared_weapon_tag_mutation_forbidden -ne $true -or
        $projectileScope.marker_direction_flag_mutation_forbidden -ne $true -or
        $projectileScope.stock_player_aim_direction_preserved -ne $true) {
    throw 'Reach native projectile-origin runtime scope is not exact local primary FP only.'
}

$hrekProjectileStart = [uint64](Convert-HexUInt32 `
    ([string]$projectileHrek.projectile_fire_function_rva) `
    'HREK projectile-fire RVA')
$hrekProjectileEnd = [uint64](Convert-HexUInt32 `
    ([string]$projectileHrek.projectile_fire_function_end_rva_exclusive) `
    'HREK projectile-fire end RVA')
$hrekOriginStart = [uint64](Convert-HexUInt32 `
    ([string]$projectileHrek.origin_consumer_start_rva) `
    'HREK projectile-origin consumer RVA')
$hrekOriginEnd = [uint64](Convert-HexUInt32 `
    ([string]$projectileHrek.origin_consumer_end_rva_exclusive) `
    'HREK projectile-origin consumer end RVA')
$hrekProjectileOwners = @($hrekSections | Where-Object {
    $_.Executable -and $hrekProjectileStart -ge $_.StartRva -and
        $hrekProjectileEnd -le $_.StartRva + $_.RawSize
})
if ($hrekProjectileOwners.Count -ne 1 -or
        $hrekProjectileEnd -le $hrekProjectileStart -or
        $hrekOriginStart -lt $hrekProjectileStart -or
        $hrekOriginEnd -le $hrekOriginStart -or
        $hrekOriginEnd -gt $hrekProjectileEnd) {
    throw 'Pinned HREK projectile-origin range is not contained in one executable function range.'
}
Write-Host ('HREK projectile origin: pinned executable range 0x{0:X8}-0x{1:X8}' -f `
    $hrekOriginStart, $hrekOriginEnd)

Test-FunctionEvidence -Bytes $moduleBytes -Sections $sections `
    -Evidence $projectileFire -Label 'retail projectile-fire transaction'
Test-ExecutableRangeEvidence -Bytes $moduleBytes -Sections $sections `
    -Evidence $projectileDecision -Label 'retail projectile-origin block'
$decisionRva = [uint64](Convert-HexUInt32 `
    ([string]$projectileDecision.decision_rva) `
    'retail projectile-origin decision RVA')
Test-ExactExecutableBytes -Bytes $moduleBytes -Sections $sections `
    -Rva $decisionRva -ExpectedBytes ([string]$projectileDecision.decision_bytes) `
    -ExpectedSha256 ([string]$projectileDecision.decision_sha256) `
    -Label 'retail projectile-origin decision'
$projectileFireRva = [uint64](Convert-HexUInt32 `
    ([string]$projectileFire.function_rva) 'retail projectile-fire RVA')
$projectileWrapperRva = [uint64](Convert-HexUInt32 `
    ([string]$projectileFire.wrapper_rva) 'retail projectile-fire wrapper RVA')
Test-Rel32CallEvidence -Bytes $moduleBytes -Sections $sections `
    -CallSiteRva ([uint64](Convert-HexUInt32 `
        ([string]$projectileFire.direct_rel32_call_site_rva) `
        'retail projectile-fire direct call site')) `
    -TargetRva $projectileFireRva -Label 'retail wrapper to projectile-fire'
foreach ($upstreamSite in $projectileFire.wrapper_upstream_rel32_call_sites) {
    Test-Rel32CallEvidence -Bytes $moduleBytes -Sections $sections `
        -CallSiteRva ([uint64](Convert-HexUInt32 ([string]$upstreamSite) `
            'retail projectile wrapper upstream call site')) `
        -TargetRva $projectileWrapperRva `
        -Label ('retail projectile wrapper upstream {0}' -f $upstreamSite)
}
Test-FunctionEvidence -Bytes $moduleBytes -Sections $sections `
    -Evidence $projectileSlot -Label 'retail output-user first-person weapon slot'
$projectileSlotRva = [uint64](Convert-HexUInt32 `
    ([string]$projectileSlot.function_rva) `
    'retail output-user first-person weapon slot RVA')
Test-Rel32CallEvidence -Bytes $moduleBytes -Sections $sections `
    -CallSiteRva ([uint64](Convert-HexUInt32 `
        ([string]$projectileSlot.first_person_consumer_rel32_call_site_rva) `
        'retail first-person weapon-slot consumer call site')) `
    -TargetRva $projectileSlotRva `
    -Label 'retail first-person consumer to weapon-slot lookup'
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
$fpRebuild = $fpCameraEvidence[0].rebuild
if ([string]$fpRebuild.nested_workspace_rva -cne '0x00CFAC20' -or
        [string]$fpRebuild.nested_compact_offset -cne '0x0000' -or
        [string]$fpRebuild.nested_secondary_derived_offset -cne '0x01E4' -or
        [string]$fpRebuild.nested_callback_offset -cne '0x02A8' -or
        [string]$fpRebuild.nested_callback_rva -cne '0x0000C380' -or
        [string]$fpRebuild.first_person_view_rva -cne '0x00BB8F68') {
    throw 'Reach FP camera nested-workspace identity is incomplete or inconsistent.'
}
Test-FunctionEvidence -Bytes $moduleBytes -Sections $sections `
    -Evidence $fpCameraEvidence[0].rebuild `
    -Label 'retail first-person camera rebuild'
Test-FunctionEvidence -Bytes $moduleBytes -Sections $sections `
    -Evidence $fpCameraEvidence[0].uploader `
    -Label 'retail first-person camera uploader'
$fpWrappers = @($fpCameraEvidence[0].visible_wrapper_transactions)
if ($fpWrappers.Count -ne 3) {
    throw 'Reach FP camera evidence must contain all three visible wrapper bodies.'
}
foreach ($wrapper in $fpWrappers) {
    if ([string]$wrapper.nested_workspace_rva -cne '0x00CFAC20' -or
            [string]$wrapper.callback_rva -cne '0x0000C380' -or
            [string]$wrapper.first_person_view_rva -cne '0x00BB8F68' -or
            [string]$wrapper.rebuild_target_rva -cne '0x00286C6C') {
        throw 'Reach FP wrapper nested-workspace identity is inconsistent.'
    }
    $wrapperBody = [pscustomobject]@{
        player_view_render_rva = $wrapper.function_rva
        player_view_render_end_rva_exclusive =
            $wrapper.function_end_rva_exclusive
        player_view_render_size = $wrapper.function_size
        player_view_render_body_sha256 = $wrapper.body_sha256
    }
    Test-FunctionBodyEvidence -Path $ModulePath -Sections $sections `
        -Evidence $wrapperBody `
        -Label ('retail first-person wrapper {0}' -f $wrapper.function_rva)
}
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
Write-Host 'Projectile origin: exact local primary-FP static proof; headset acceptance pending'
Write-Host 'Camera runtime:   permanent transaction audited separately; headset acceptance pending'
