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
    'hidden right-arm ownership admitted into the visible keep mask' =
        'const\s+uint64_t\s+keep\s*=[^;]*rightControllerOwnedSourceBranch'
    'hidden left-arm branch receives the visible-hand rigid delta' =
        'if\s*\(!\(\s*leftControllerOwnedSourceBranch\s*&'
    'prepared controller targets rebased through the render head root' =
        'ReachRebasePreparedControllerTargets'
    'placement base retained for a second controller-target translation' =
        'placementBase(?:Valid)?'
    'prepared wrist translation rebuilt from the palette root' =
        'target\.translation\[axis\]\s*=\s*renderRoot\.translation\[axis\]'
    'shared weapon-tag projectile-origin mutation' =
        'SafeWrite(?:Byte|Bytes)[\s\S]{0,160}kReachBarrelProjectilesUseWeaponOriginMask'
    'projectile marker-direction override' =
        'ReachFpProjectileOrigin[\s\S]{0,240}kReachBarrelProjectileFiresInMarkerDirectionMask'
    'generic projectile-function entry detour' =
        'MH_CreateHook\(\s*reinterpret_cast<void\*>\(base\s*\+\s*kReachProjectileFireRva\)'
    'marker composer limited to the expired outer pair scope' =
        'ComposeBoneMatrices\(\s*g_reachFpPairScope\.targets\.centerRoot,\s*markerRecordSpace,markerWorld\)'
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
    'Reach hidden right influence branch derived from exact ownership' =
        'const\s+uint64_t\s+hiddenRight=\s*context\.layout\.rightControllerOwnedSourceBranch&\s*~fp\.wristDescendants'
    'Reach hidden influences collapse at solved left wrist' =
        'BoneMatrix\s+collapsedAtLeftWrist=\s*g_fpPaletteScratch\[fp\.lWrist\];\s*collapsedAtLeftWrist\.scale=0\.0001f;'
    'Reach hidden influences collapse at solved right wrist' =
        'BoneMatrix\s+collapsedAtRightWrist=\s*g_fpPaletteScratch\[fp\.wrist\];\s*collapsedAtRightWrist\.scale=0\.0001f;'
    'Reach hidden influence records use wrist collapse anchor' =
        'if\s*\(i<64\s*&&\s*\(hiddenLeft&\(uint64_t\{1\}<<i\)\)\)\s*\{\s*g_fpPaletteScratch\[i\]=\s*collapsedAtLeftWrist;'
    'Reach hidden right influence records use wrist collapse anchor' =
        'else\s+if\s*\(i<64\s*&&\s*\(hiddenRight&\(uint64_t\{1\}<<i\)\)\)\s*\{\s*g_fpPaletteScratch\[i\]=\s*collapsedAtRightWrist;'
    'Reach hidden wrist anchoring excludes hands and held objects' =
        'if\s*\(!hand\s*&&\s*!held\)\s*\{[\s\S]*?hiddenLeft[\s\S]*?hiddenRight'
    'Reach layout identity includes right controller ownership' =
        'a\.rightControllerOwnedSourceBranch==\s*b\.rightControllerOwnedSourceBranch'
    'Reach layout identity includes left controller ownership' =
        'a\.leftControllerOwnedSourceBranch==\s*b\.leftControllerOwnedSourceBranch'
    'prepared controller target is absolute gameplay-base plus room offset' =
        'out\.translation\[axis\]\s*=\s*gameplayBase\[axis\]\s*\+\s*offset\[axis\]\s*\+\s*basis\[axis\]\s*\*\s*standoff'
    'prepared right target uses the pre-head gameplay base' =
        'ReachBuildPreparedControllerTarget\(\s*tracking,false,candidate\.gameplayBasePosition'
    'prepared left target uses the pre-head gameplay base' =
        'ReachBuildPreparedControllerTarget\(\s*tracking,true,candidate\.gameplayBasePosition'
    'marker path consumes the prepared absolute targets directly' =
        'FpExplicitPoseTargets\s+markerTargets=context\.targets;\s*BoneMatrix\s+alignedRight\{\};\s*if\s*\(!ReachAlignRightTargetToAuthoredBarrel\(\s*markerTargets\.rightWrist'
    'palette path consumes the prepared absolute targets directly' =
        'FpExplicitPoseTargets\s+targets=context\.targets;\s*targets\.centerRoot\.scale=root->scale;\s*BoneMatrix\s+alignedRight\{\};\s*if\s*\(!ReachAlignRightTargetToAuthoredBarrel\(\s*targets\.rightWrist'
    'Reach native projectile-origin decision target lifecycle' =
        'g_reachCamera\.fpProjectileOriginTarget'
    'Reach final authored marker-composer target lifecycle' =
        'g_reachCamera\.fpMarkerComposeTarget'
    'Reach HREK-exact assault-rifle primary-trigger admission' =
        'ReachFpMarkerCompose[\s\S]*?ReachIsHrekAssaultRiflePrimaryTrigger\(markerRecord\)'
    'Reach marker composer calls exact marker-query function' =
        'ReachVerifyRel32Call\(\s*base,kReachFpMarkerComposeQueryCallRva,\s*kReachFpMarkerQueryRva\)'
    'Reach marker composer calls exact matrix-compose function' =
        'ReachVerifyRel32Call\(\s*base,kReachFpMarkerComposeMatrixCallRva,\s*kReachFpMarkerMatrixComposeRva\)'
    'Reach shared marker transform published by the exact nested interpolation' =
        'ReachPublishMarkerSharedTransform\(\s*context\.generation,markerTargets\.centerRoot,\s*context\.untouchedLive\[context\.layout\.rightWristSource\],\s*alignedRight,markerTargets\.rightScale\)'
    'Reach query snapshots nested source ownership before stock execution' =
        'sourceSerialBefore=g_reachFpMarkerSourceSerial'
    'Reach query uses nested ownership or one fallback, never both' =
        'if\s*\(g_reachFpMarkerSourceSerial!=sourceSerialBefore\)[\s\S]*?else if\s*\(ReachApplyPublishedMarkerQueryTransform\(output\)\)'
    'Reach composer requires this query correction serial' =
        'g_reachFpMarkerQueryCorrectedSerial!=correctedSerialBefore'
    'Reach composed marker loads the published center root' =
        'LoadAtomicBoneMatrix\(\s*g_reachFpMarkerSharedTransform\.centerRoot,centerRoot\)'
    'Reach composed marker converted through the published center root' =
        'ComposeBoneMatrices\(\s*centerRoot,markerRecordSpace,markerWorld\)'
    'Reach composed marker weapon datum publication' =
        'g_reachFpPrimaryTriggerWorld\.weaponDatum\.store\(\s*weaponDatum'
    'Reach composed world translation enters native firing frame' =
        'memcpy\(frame\+barrelOffset\+0x9F0,\s*markerWorld\.translation'
    'Reach exact output-user-0 primary FP weapon gate' =
        'firstPersonSlot\s*=\s*slotForDatum\(0,\s*weaponDatum\)'
    'Reach projectile-origin callback quiescence wrapper' =
        'ReachFpProjectileOriginPredicate[\s\S]*?activeCallbacks\.fetch_add[\s\S]*?__finally[\s\S]*?activeCallbacks\.fetch_sub'
    'Reach current projectile-frame weapon datum relay load' =
        '0x8B,0x8C,0x24,0x34,0x01,0x00,0x00'
    'Reach native decision midhook creation' =
        'MH_CreateHook\(\s*fpProjectileOrigin,fpProjectileOriginRelay'
    'Reach disabled trampoline branch verification' =
        'ReachColdVerifyFpProjectileOriginTrampoline'
    'Reach projectile-origin hook enabled last' =
        'MH_EnableHook\(fpProjectileOrigin\)'
    'Reach marker-composer hook enabled' =
        'MH_EnableHook\(fpMarkerCompose\)'
    'Reach serial-gated marker-query hook enabled' =
        'MH_EnableHook\(fpMarkerQuery\)'
    'Reach serial-gated marker-query target lifecycle' =
        'g_reachCamera\.fpMarkerQueryTarget'
    'Reach projectile-origin relay ingress scan' =
        'instruction\s*>=\s*projectileRelay\s*&&\s*instruction\s*<\s*projectileRelayEnd'
    'Reach projectile-origin relay released after hook removal' =
        'projectileOriginRemoved\s*&&[\s\S]*?VirtualFree\('
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
    'exact hidden right-arm influence source mask' =
        'kReachRightControllerOwnedAuxiliarySourceMask\s*=\s*0x0000000000004640ull'
    'resolved right controller ownership union' =
        'layout\.rightControllerOwnedSourceBranch\s*=\s*rightSourceMask\s*\|\s*kReachRightControllerOwnedAuxiliarySourceMask'
    'resolved left controller ownership union' =
        'layout\.leftControllerOwnedSourceBranch\s*=\s*leftSourceMask\s*\|\s*kReachLeftControllerOwnedAuxiliarySourceMask'
    'Reach native projectile-origin decision RVA' =
        'kReachProjectileOriginDecisionRva\s*=\s*0x004C30C5'
    'Reach native projectile-origin true continuation' =
        'kReachProjectileOriginNativeTrueRva\s*=\s*0x004C30D4'
    'Reach exact FP weapon-slot helper RVA' =
        'kReachFpWeaponSlotForDatumRva\s*=\s*0x002B1218'
    'Reach local primary FP projectile-origin policy' =
        'controllerAimActive\s*&&\s*firstPersonWeaponSlot\s*==\s*0'
    'Reach weapon-origin and marker-direction bits remain distinct' =
        'kReachBarrelProjectileFiresInMarkerDirectionMask'
    'Reach final marker-composer RVA' =
        'kReachFpMarkerComposeRva\s*=\s*0x0011BFB0'
    'Reach marker-composer marker-query call edge' =
        'kReachFpMarkerComposeQueryCallRva\s*=\s*0x0011BFDC'
    'Reach marker-composer matrix-compose call edge' =
        'kReachFpMarkerComposeMatrixCallRva\s*=\s*0x0011BFED'
    'Reach marker matrix-compose target' =
        'kReachFpMarkerMatrixComposeRva\s*=\s*0x000A90D8'
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

Write-Host 'Reach FP Halo 3/ODST palette + native weapon-IK + world-projection camera + composed primary-trigger projectile-origin parity gate passed.'
