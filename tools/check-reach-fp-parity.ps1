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
    'outer stereo workspace reused for FP camera upload' =
        'scope\.workspace\s*\+\s*kReachSecondaryDerivedOffset'
    'legacy unverified FP compact-camera workspace alias' =
        'kReachFpCompactCameraRva'
    'FP camera success published before the uploader returns' =
        'ReachFpCameraRebuildBody[\s\S]*?PublishReachFpCameraUpload\(scope\)[\s\S]*?g_reachFpCameraUpload\(compact,\s*derived\)'
    'hidden left-arm ownership admitted into the visible keep mask' =
        'const\s+uint64_t\s+keep\s*=[^;]*leftControllerOwnedSourceBranch'
    'hidden left-arm branch receives the visible-hand rigid delta' =
        'if\s*\(!\(\s*leftControllerOwnedSourceBranch\s*&'
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
    'exact nested first-person camera workspace selection' =
        'SelectReachFpCameraNestedWorkspace\(\s*base,\s*g_reachCamera\.size,\s*topWorkspace,\s*workspaceCallback,\s*reinterpret_cast<uintptr_t>\(view\)\s*\)'
    'nested first-person camera callback load' =
        'base\s*\+\s*kReachFpCameraWorkspaceRva\s*\+\s*kReachFpCameraWorkspaceCallbackOffset'
    'nested first-person compact destination' =
        'compact\s*=\s*reinterpret_cast<void\*>\(\s*nestedWorkspace\s*\)'
    'nested first-person derived destination' =
        'derived\s*=\s*reinterpret_cast<void\*>\(\s*nestedWorkspace\s*\+\s*kReachSecondaryDerivedOffset\s*\)'
    'post-upload first-person camera success publication' =
        'g_reachFpCameraUpload\(compact,\s*derived\);\s*PublishReachFpCameraUpload\(scope\)'
    'both-eye first-person camera success gate' =
        '\(eyeMask\s*&\s*0x3u\)\s*!=\s*0x3u'
    'worker-owned first-person camera success publication' =
        'LogReachFpCameraUploadIfReady\(\);\s*LogReachFpStatusIfNew\(\)'
    'exact both-eye first-person camera success log' =
        'LOG\("Reach per-eye FP camera ACTIVE:'
    'Reach private-palette left controller binding' =
        'ReachBindFloatingLeftHandToController\(\s*\*root,fp,\s*context\.layout\.leftControllerOwnedSourceBranch,\s*targets\)'
    'Reach forced floating-hands presentation' =
        'Reach ignores floating_hands config'
    'Reach exact left-hand-only controller delta' =
        'if\s*\(!\(\s*fp\.lWristDescendants\s*&\s*\(uint64_t\{1\}\s*<<\s*node\)\s*\)'
    'Reach hidden left-arm branch excluded from visibility' =
        'const\s+uint64_t\s+keep=fp\.wristDescendants\|\s*fp\.lWristDescendants'
    'Reach hidden influence branch derived from exact ownership' =
        'const\s+uint64_t\s+hiddenLeft=\s*context\.layout\.leftControllerOwnedSourceBranch&\s*~fp\.lWristDescendants'
    'Reach hidden influences collapse at solved left wrist' =
        'BoneMatrix\s+collapsedAtLeftWrist=\s*g_fpPaletteScratch\[fp\.lWrist\];\s*collapsedAtLeftWrist\.scale=0\.0001f;'
    'Reach hidden influence records use wrist collapse anchor' =
        'if\s*\(i<64\s*&&\s*\(hiddenLeft&\(uint64_t\{1\}<<i\)\)\)\s*\{\s*g_fpPaletteScratch\[i\]=\s*collapsedAtLeftWrist;'
    'Reach layout identity includes left controller ownership' =
        'a\.leftControllerOwnedSourceBranch==\s*b\.leftControllerOwnedSourceBranch'
}
foreach ($entry in $requiredGame.GetEnumerator()) {
    if ($game -notmatch $entry.Value) {
        throw "Reach FP parity gate missing: $($entry.Key)."
    }
}
$requiredLogic = [ordered]@{
    'pure nested first-person camera workspace selector' =
        'SelectReachFpCameraNestedWorkspace'
    'exact nested first-person camera workspace identity' =
        'stackTop\s*==\s*expectedWorkspace'
    'exact nested first-person camera callback identity' =
        'workspaceCallback\s*==\s*moduleBase\s*\+\s*kReachFpCameraWorkspaceCallbackRva'
    'exact nested first-person camera view identity' =
        'fpView\s*==\s*moduleBase\s*\+\s*kReachFpCameraViewRva'
    'first-person wrapper body hashes in complete proof' =
        'proof\.fpCameraWrapperBodyHashes'
    'exact hidden left-arm influence source mask' =
        'kReachLeftControllerOwnedAuxiliarySourceMask\s*=\s*0x00000000000011A0ull'
    'resolved left controller ownership union' =
        'layout\.leftControllerOwnedSourceBranch\s*=\s*leftSourceMask\s*\|\s*kReachLeftControllerOwnedAuxiliarySourceMask'
}
foreach ($entry in $requiredLogic.GetEnumerator()) {
    if ($logic -notmatch $entry.Value) {
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
