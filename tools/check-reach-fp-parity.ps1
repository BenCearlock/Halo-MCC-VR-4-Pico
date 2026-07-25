[CmdletBinding()]
param()

# Static packaging gate for the strict Halo 3/ODST first-person transaction
# contract. This is intentionally narrow: it rejects the exact Reach-only
# architectures already disproven in-headset and requires the source-level
# invariants that keep every final palette on the shared reconstruction path
# and bypass the title's native flat-screen support-hand weapon IK.

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
    'title-native weapon IK disable control' =
        'debug_animation_fp_weapon_ik_disable'
    'all-or-nothing native weapon IK bypass' =
        'bool\s+InstallReachCameraCore[\s\S]*?if\s*\(!ApplyReachNativeWeaponIkBypass\(\)\)'
    'native weapon IK lifecycle restore' =
        'bool\s+RemoveReachCameraCore[\s\S]*?if\s*\(!RestoreReachNativeWeaponIkBypass\(\)\)'
    'stock first-person camera rebuild retained' =
        'original\(view, firstPersonEnabled\)'
    'per-eye world compact camera substitution' =
        'memcpy\(compact, scope\.compact, sizeof\(scope\.compact\)\)'
    'per-eye world derived projection substitution' =
        'memcpy\(derived, scope\.derived, sizeof\(scope\.derived\)\)'
    'stock first-person camera constant re-upload' =
        'g_reachFpCameraUpload\(compact, derived\)'
    'first-person camera hook lifecycle target' =
        'g_reachCamera\.fpCameraTarget'
    'Reach private-palette left controller binding' =
        'ReachBindFloatingLeftHandToController\(\*root,fp,targets\)'
    'Reach forced floating-hands presentation' =
        'Reach ignores floating_hands config'
    'Reach exact left-hand source mask only' =
        'fp\.lWristDescendants&\(uint64_t\{1\}<<node\)'
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
if ($logic -notmatch 'kReachFpWeaponIkDisableValueRva' -or
    $logic -notmatch 'kReachFpWeaponIkDisabledEpilogueRva') {
    throw 'Reach FP parity gate missing: exact Reach native weapon-IK proof anchors.'
}
if ($logic -notmatch 'kReachFpCameraRebuildAob' -or
    $logic -notmatch 'kReachFpCameraUploadAob' -or
    $logic -notmatch 'exactFpCameraFlowEdges') {
    throw 'Reach FP parity gate missing: exact Reach camera rebuild/upload proof anchors.'
}

Write-Host 'Reach FP Halo 3/ODST palette + native weapon-IK + world-projection camera parity gate passed.'
