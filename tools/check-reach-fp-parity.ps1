[CmdletBinding()]
param()

# Static packaging gate for the strict Halo 3/ODST first-person transaction
# contract. This is intentionally narrow: it rejects the exact Reach-only
# architectures already disproven in-headset and requires the source-level
# invariants that keep every final palette on the shared reconstruction path.

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$gamePath = Join-Path $repoRoot 'src\dll\game.cpp'
$logicPath = Join-Path $repoRoot 'src\common\reach_render_logic.h'
$agentsPath = Join-Path $repoRoot 'AGENTS.md'
$game = [IO.File]::ReadAllText($gamePath)
$logic = [IO.File]::ReadAllText($logicPath)
$agents = [IO.File]::ReadAllText($agentsPath)

$forbidden = [ordered]@{
    'single Reach interpolation context' =
        'thread_local\s+ReachFpInterpolationContext\s+g_reachFpInterpolation\s*;'
    'separated live-graph hand transform' = 'ReachApplySeparatedHandGraph'
    'Reach-only source ownership enum' = 'ReachFpSourceOwner'
    'body-only palette action' = 'ArticulateExactBody'
    'palette solve truncated to discovery count' =
        'fp\.count\s*=\s*static_cast<int>\(observed\.paletteBodyNodeCount\)'
}
foreach ($entry in $forbidden.GetEnumerator()) {
    if (($game + "`n" + $logic) -match $entry.Value) {
        throw "Reach FP parity gate rejected: $($entry.Key)."
    }
}

$requiredGame = [ordered]@{
    'bounded per-transaction contexts' =
        'g_reachFpInterpolations\[kReachFpTransactionCapacity\]'
    'newest exact source-pointer palette match' =
        'candidate\.captureSerial>matched->captureSerial'
    'full live-source reconstruction count' =
        'fp\.count\s*=\s*context\.liveSourceCount'
    'exact appended held-object boundary' =
        'fp\.heldObjectStart\s*=\s*static_cast<int>\([\s\S]*?context\.layout\.paletteBodyNodeCount\)'
    'private untouched source reconstruction' =
        'ReconstructVisiblePaletteSource\([\s\S]*?context\.untouchedLive\)'
}
foreach ($entry in $requiredGame.GetEnumerator()) {
    if ($game -notmatch $entry.Value) {
        throw "Reach FP parity gate missing: $($entry.Key)."
    }
}
if ($logic -notmatch 'ArticulateKnownTransaction') {
    throw 'Reach FP parity gate missing: every known final palette action.'
}
if ($agents -notmatch 'Strict implementation-parity rule') {
    throw 'Reach FP parity gate missing: repository parity contract.'
}

Write-Host 'Reach FP Halo 3/ODST transaction parity gate passed.'