[CmdletBinding()]
param()

# Static packaging gate for the strict Halo 3/ODST first-person transaction
# contract. This is intentionally narrow: it rejects the exact Reach-only
# architectures already disproven in-headset and requires the source-level
# invariants that keep every final palette on the shared reconstruction path
# and bypass the title's native flat-screen support-hand weapon IK. It also
# requires the official-HREK class-2 authored-crosshair bridge as one mandatory,
# allocation-free Reach eye transaction with no procedural/name/RVA fallback.

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$gamePath = Join-Path $repoRoot 'src\dll\game.cpp'
$vrPath = Join-Path $repoRoot 'src\dll\vr.cpp'
$logicPath = Join-Path $repoRoot 'src\common\reach_render_logic.h'
$chudLogicPath = Join-Path $repoRoot 'src\common\reach_chud_logic.h'
$titleRegistryPath = Join-Path $repoRoot 'src\common\title_registry.cpp'
$agentsPath = Join-Path $repoRoot 'AGENTS.md'
$game = [IO.File]::ReadAllText($gamePath)
$vr = [IO.File]::ReadAllText($vrPath)
$logic = [IO.File]::ReadAllText($logicPath)
$chudLogic = [IO.File]::ReadAllText($chudLogicPath)
$titleRegistry = [IO.File]::ReadAllText($titleRegistryPath)
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
    'Reach projectile-origin hook or relay' =
        'ReachFpProjectileOrigin|fpProjectileOrigin(?:Target|Relay)|kReachProjectile(?:Fire|Origin)'
    'Reach projectile-only weapon-slot gate' =
        '(?:g_|k)reachFpWeaponSlotForDatum|ReachShouldUseNativeWeaponProjectileOrigin'
    'Reach first-person marker-query or marker-composer detour' =
        'ReachFpMarker(?:Query|Compose)|fpMarker(?:Query|Compose)Target|kReachFpMarker(?:Query|Compose)'
    'Reach published marker fallback' =
        'ReachApplyPublishedMarkerQueryTransform|ReachPublishMarkerSharedTransform|g_reachFpMarker(?:SharedTransform|SourceSerial|QueryCorrectedSerial)'
    'Reach primary-trigger firing-frame write' =
        'g_reachFpPrimaryTriggerWorld|frame\+barrelOffset\+0x9F0'
    'Reach projectile tag-policy constants' =
        'kReachBarrelProjectilesUseWeaponOriginMask|kReachBarrelProjectileFiresInMarkerDirectionMask'
}
foreach ($entry in $forbidden.GetEnumerator()) {
    if (($game + "`n" + $logic) -match $entry.Value) {
        throw "Reach FP parity gate rejected: $($entry.Key)."
    }
}

$forbiddenChud = [ordered]@{
    'optional Reach authored-crosshair bridge' =
        'Optional,\s*HREK-only authored-crosshair bridge|if\s*\(hudDrawWidget\)\s*\{'
    'optional Reach CHUD hook target publication' =
        'hudDrawWidgetCreated\s*\?\s*hudDrawWidget\s*:\s*nullptr'
    'optional Reach CHUD hook enable' =
        'hudDrawWidgetCreated\s*&&\s*MH_EnableHook\(hudDrawWidget\)'
    'Reach CHUD widget-name selection or fallback' =
        'ReachHudDrawWidgetDetour[\s\S]{0,3600}(?:strcmp|strstr|widgetName|artistName)'
    'Reach procedural reticle generation from the CHUD detour' =
        'ReachHudDrawWidgetDetour[\s\S]{0,3600}(?:PaintReticle|EnsureReticleChain|reticle_r|reticle_g|reticle_b)'
    'Reach hardcoded shipping CHUD RVA' =
        'base\s*\+\s*kReach\w*Chud\w*Rva'
    'Reach procedural or approximate CHUD action' =
        'ReachChudCrosshairAction::(?:Procedural|DrawProcedural|Fallback|Approximate)'
    'Reach projection queued before authored upload result' =
        'layers\.push_back\(\s*reinterpret_cast<XrCompositionLayerBaseHeader\*>\(\s*&projection\s*\)\s*\)[\s\S]{0,2200}?authoredUploadFailed'
    'Reclaimer-derived Reach CHUD binding' =
        'Reclaimer|kReachRetailChud'
    'Reach-specific procedural reticle enable' =
        '(?:GameTitle::HaloReach|reachTitle)[\s\S]{0,320}(?:kProceduralOpacity|PaintReticle|reticle_r)'
    'owned Reach transaction stock rerender helper' =
        'ReachScopedStock(?:Fallback|BeforeOwnership)'
    'failed claimed Reach transaction invokes the stock renderer' =
        'if\s*\(\s*!handled\s*\)[\s\S]{0,260}?(?:ReachScopedStock|g_reachOrigPlayerViewRender)'
}
foreach ($entry in $forbiddenChud.GetEnumerator()) {
    if (($game + "`n" + $chudLogic + "`n" + $vr) -match $entry.Value) {
        throw "Reach authored-crosshair parity gate rejected: $($entry.Key)."
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
}
foreach ($entry in $requiredGame.GetEnumerator()) {
    if ($game -notmatch $entry.Value) {
        throw "Reach FP parity gate missing: $($entry.Key)."
    }
}

$requiredChudGame = [ordered]@{
    'mandatory HREK authored-crosshair resolver gate' =
        'if\s*\(\s*!ResolveReachHrekChudDrawWidget\(\s*base,\s*size,\s*hudDrawWidget\s*\)\s*\)[\s\S]{0,480}?return false'
    'failed HREK parity generation cannot churn reinstall attempts' =
        'g_reachChudParityFailedGeneration\.load\([\s\S]{0,160}?\)\s*==\s*generation\s*\)[\s\S]{0,100}?return false'
    'HREK resolver failure latches the rejected generation' =
        '!ResolveReachHrekChudDrawWidget\([\s\S]{0,180}?g_reachChudParityFailedGeneration\.store\(\s*generation,\s*std::memory_order_release\s*\)'
    'loaded-image HREK CHUD class anchor has an unwind owner' =
        'void\s+InspectReachHrekChudAbiVariant\([\s\S]{0,1200}?sig::Find\([\s\S]{0,520}?RtlLookupFunctionEntry\('
    'loaded-image HREK CHUD unwind body is bounded' =
        'functionBegin\s*<\s*base[\s\S]{0,420}?functionBegin\s*>\s*hit[\s\S]{0,420}?functionEnd\s*>\s*end[\s\S]{0,420}?functionEnd\s*-\s*functionBegin\s*<\s*0x200[\s\S]{0,180}?functionEnd\s*-\s*functionBegin\s*>\s*0x800'
    'HREK CHUD descriptor fifth argument class and nonrecursion proof' =
        'CountReachHrekChudBytes\(body,\s*descriptorMove,\s*0x100\)\s*!=\s*1[\s\S]{0,180}?CountReachHrekChudBytes\(body,\s*fifthArgumentLoad,\s*0x100\)\s*!=\s*1[\s\S]{0,180}?CountReachHrekChudBytes\(body,\s*classRead,\s*body\.size\(\)\)\s*!=[\s\S]{0,80}?expectedClassReadCount[\s\S]{0,180}?ReachHrekChudHasDirectSelfEntry\(body,\s*functionBegin\)'
    'all three official HREK CHUD ABI variants are admitted together' =
        'InspectReachHrekChudAbiVariant\(\s*base,\s*size,\s*kTagTestClassReadAob,[\s\S]{0,360}?kTagTestClassRead,\s*4,\s*kTagTestFifthArgumentLoad[\s\S]{0,360}?InspectReachHrekChudAbiVariant\(\s*base,\s*size,\s*kTagPlayClassReadAob,[\s\S]{0,360}?kTagClassRead,\s*1,\s*kFifthArgumentLoad[\s\S]{0,360}?InspectReachHrekChudAbiVariant\(\s*base,\s*size,\s*kSapienPlayClassReadAob[\s\S]{0,360}?kSapienClassRead,\s*1,\s*kFifthArgumentLoad'
    'exactly one HREK CHUD ABI owner is mandatory' =
        'matches\.malformed\s*\|\|\s*matches\.candidateCount\s*!=\s*1\s*\|\|[\s\S]{0,80}?!matches\.candidate'
    'Reach CHUD exact stereo-eye transaction ownership' =
        'bool\s+ReachOwnsHudStereoTransaction\(\)[\s\S]{0,900}?TitleAdapter_GetActiveTitle\(\)\s*!=\s*GameTitle::HaloReach[\s\S]{0,260}?cameraGeneration\s*=\s*g_reachCamera\.generation\.load[\s\S]{0,260}?TitleAdapter_GetGeneration\(GameTitle::HaloReach\)\s*==\s*cameraGeneration[\s\S]{0,220}?installed\.load[\s\S]{0,220}?armed\.load[\s\S]{0,220}?teardownRequested\.load[\s\S]{0,300}?VR_IsStereoEnabled\(\)[\s\S]{0,220}?g_reachFpCameraEyeScope\.active[\s\S]{0,220}?g_reachFpCameraEyeScope\.generation\s*==\s*cameraGeneration'
    'in-flight Reach eye ownership loss invalidates and suppresses CHUD' =
        'matchingEyeScope\s*&&\s*!ownsStereo[\s\S]{0,520}?g_reachFpCameraEyeScope\.chudParityFailed\s*=\s*true\s*;[\s\S]{0,80}?return\s*;'
    'Reach authored-reticle compositor ownership remains narrow' =
        'bool\s+Game_OwnsReachAuthoredReticle\(\)[\s\S]{0,900}?TitleAdapter_GetActiveTitle\(\)\s*!=\s*GameTitle::HaloReach[\s\S]{0,260}?cameraGeneration\s*=\s*g_reachCamera\.generation\.load[\s\S]{0,260}?TitleAdapter_GetGeneration\(GameTitle::HaloReach\)\s*==\s*cameraGeneration[\s\S]{0,180}?installed\.load[\s\S]{0,180}?armed\.load[\s\S]{0,180}?teardownRequested\.load[\s\S]{0,220}?g_enabled\.load[\s\S]{0,180}?VR_IsStereoEnabled\(\)'
    'Reach CHUD callback counter and finally quiescence' =
        'ReachHudDrawWidgetDetour[\s\S]{0,480}?activeCallbacks\.fetch_add[\s\S]{0,4200}?__finally[\s\S]{0,360}?activeCallbacks\.fetch_sub'
    'Reach scope transaction suppresses every CHUD widget' =
        'ownsStereo\s*&&[\s\S]{0,100}?g_scopeRenderActive\.load\([\s\S]{0,100}?\)\s*\{[\s\S]{0,240}?return'
    'safe descriptor plus-four scripting-class read' =
        'descriptor\s*&&\s*SafeReadByte\(\s*reinterpret_cast<const uint8_t\*>\(descriptor\)\s*\+\s*4,\s*&rawClass\s*\)'
    'universal crosshair configuration transaction' =
        'ReachDecideChudCrosshairAction\([\s\S]{0,260}?g_config\.crosshair,[\s\S]{0,100}?g_config\.kill_reticle,[\s\S]{0,160}?g_config\.right_eye_first\s*\)'
    'authored class-2 capture wraps the stock draw' =
        'action\s*==\s*ReachChudCrosshairAction::CaptureAuthored[\s\S]{0,260}?VR_BeginAuthoredReticleCapture\(\)[\s\S]{0,180}?captureStarted\s*=\s*true[\s\S]{0,180}?original\(userIndex,\s*descriptor,\s*widgetIndex'
    'owned class-2 emission is recorded on the exact eye scope' =
        'if\s*\(ownsStereo\s*&&\s*isCrosshairClass\)[\s\S]{0,120}?g_reachFpCameraEyeScope\.chudClass2Seen\s*=\s*true'
    'single current-eye rejection helper closes the transaction' =
        'void\s+RejectReachChudParityForCurrentEye\(\)\s*noexcept[\s\S]{0,520}?g_reachFpCameraEyeScope\.chudParityFailed\s*=\s*true[\s\S]{0,180}?g_reachChudParityFailedGeneration\.store\([\s\S]{0,180}?armed\.store\(false[\s\S]{0,180}?teardownRequested\.store\(\s*true'
    'RejectTransaction action invokes current-eye rejection' =
        'action\s*==\s*ReachChudCrosshairAction::RejectTransaction[\s\S]{0,180}?RejectReachChudParityForCurrentEye\(\)[\s\S]{0,100}?return'
    'failed eye suppresses later class-2 draws in the same transaction' =
        'matchingEyeScope\s*&&\s*g_reachFpCameraEyeScope\.chudParityFailed\s*&&\s*isCrosshairClass[\s\S]{0,100}?return'
    'runtime authored-target loss rejects the parity generation' =
        'VR_BeginAuthoredReticleCapture\(\)[\s\S]{0,520}?captureStarted\s*=\s*true[\s\S]{0,1100}?RejectReachChudParityForCurrentEye\(\)[\s\S]{0,100}?return'
    'failed CHUD eye cannot publish a Reach raster' =
        'if\s*\(\s*!renderReturned\s*\|\|\s*fpCameraScope\.chudParityFailed\s*\)[\s\S]{0,180}?transactionValid\s*=\s*false[\s\S]{0,100}?break[\s\S]{0,900}?VR_ReachCopyEye'
    'every admitted Reach attempt invalidates prior authored art' =
        'g_reachRenderFovSerial\[0\]\.store\(0,[\s\S]{0,220}?g_reachRenderFovSerial\[1\]\.store\(0,[\s\S]{0,220}?VR_InvalidatePreparedReachAuthoredReticleCapture\(\)'
    'authored capture closes in finally' =
        '__finally[\s\S]{0,180}?if\s*\(captureStarted\)[\s\S]{0,80}?VR_EndPreparedAuthoredReticleCapture\(\)'
    'prepared authored capture completion is recorded or rejected' =
        'if\s*\(VR_EndPreparedAuthoredReticleCapture\(\)\)[\s\S]{0,160}?authoredCrosshairCaptured\s*=\s*true[\s\S]{0,160}?else[\s\S]{0,100}?RejectReachChudParityForCurrentEye\(\)'
    'class-2 pair proof rejects before final Reach eye copy' =
        'if\s*\(pass\s*==\s*1\s*&&\s*!ReachAuthoredCrosshairPairComplete\([\s\S]{0,260}?RejectReachChudParityForCurrentEye\(\)[\s\S]{0,180}?break[\s\S]{0,180}?VR_ReachCopyEye'
    'claimed Reach transaction failure is terminal' =
        'if\s*\(\s*!handled\s*\)[\s\S]{0,120}?Game_RejectReachAuthoredReticle\(epoch\.generation\)'
    'unclaimed Reach inner calls remain native like Halo 3 and ODST' =
        '!ReachInnerScopeMatchesLive\(playerView,\s*returnAddress\)[\s\S]{0,520}?g_reachOrigPlayerViewRender\(playerView\)[\s\S]{0,80}?return'
    'exact owned scope disarm is terminal without flat rerender' =
        'if\s*\(\s*!g_reachCamera\.armed\.load[\s\S]{0,420}?Game_RejectReachAuthoredReticle\([\s\S]{0,180}?return'
    'Reach uses the shared Halo 3/ODST render-thread authored capture path' =
        'VR_BeginAuthoredReticleCapture\(\)'
    'Reach install polling waits for authored-resource runtime readiness' =
        'VR_ReachDisplayReady\(epoch\)\s*&&\s*VR_CanPrepareAuthoredReticleResources\(\)'
    'mandatory CHUD hook creation follows the camera hooks' =
        'const\s+bool\s+hudDrawWidgetCreated\s*=\s*fpCameraCreated\s*&&\s*MH_CreateHook\([\s\S]{0,220}?ReachHudDrawWidgetDetour[\s\S]{0,180}?g_reachOrigHudDrawWidget'
    'mandatory CHUD hook creation failure rejects the complete transaction' =
        '!fpCameraCreated\s*\|\|\s*!hudDrawWidgetCreated\s*\)[\s\S]{0,4200}?return false'
    'mandatory CHUD target publication' =
        'g_reachCamera\.hudDrawWidgetTarget\s*=\s*hudDrawWidget\s*;'
    'mandatory CHUD hook enable' =
        'MH_EnableHook\(fpCamera\)\s*!=\s*MH_OK\s*\|\|\s*MH_EnableHook\(hudDrawWidget\)\s*!=\s*MH_OK'
    'Reach CHUD detour participates in ingress scan' =
        'ReachDetourCodeRange\s+ranges\[6\][\s\S]{0,520}?ReachHudDrawWidgetDetour'
    'Reach CHUD target and trampoline participate in ingress scan' =
        'g_reachCamera\.hudDrawWidgetTarget,[\s\S]{0,900}?g_reachOrigHudDrawWidget'
    'Reach CHUD hook disables before quiescence' =
        'void\*\s+const\s+targets\[\]\s*=\s*\{\s*g_reachCamera\.hudDrawWidgetTarget,[\s\S]{0,700}?WaitForReachDetourQuiescence\(\)'
    'Reach CHUD hook removal clears target and trampoline' =
        'MH_RemoveHook\(g_reachCamera\.hudDrawWidgetTarget\)[\s\S]{0,300}?g_reachCamera\.hudDrawWidgetTarget\s*=\s*nullptr\s*;[\s\S]{0,120}?g_reachOrigHudDrawWidget\s*=\s*nullptr\s*;'
    'Reach CHUD teardown clears retained state before module release' =
        'g_reachCamera\.hudDrawWidgetTarget\s*=\s*nullptr\s*;[\s\S]{0,1800}?g_reachOrigHudDrawWidget\s*=\s*nullptr\s*;[\s\S]{0,1400}?FreeLibrary\(moduleReference\)'
    'frame-upload rejection latches and immediately disarms Reach' =
        'void\s+Game_RejectReachAuthoredReticle\(uint32_t\s+expectedGeneration\)[\s\S]{0,900}?!expectedGeneration[\s\S]{0,180}?TitleAdapter_GetActiveTitle\(\)\s*!=\s*GameTitle::HaloReach[\s\S]{0,220}?TitleAdapter_GetGeneration\(GameTitle::HaloReach\)\s*!=\s*expectedGeneration[\s\S]{0,260}?g_reachChudParityFailedGeneration\.store\(\s*expectedGeneration[\s\S]{0,220}?armed\.store\(false[\s\S]{0,180}?teardownRequested\.store\(true'
}
$reachChudCoreActive = $game -match
    'MH_CreateHook\([\s\S]{0,180}?ReachHudDrawWidgetDetour'
$inactiveChudCoreChecks = @(
    'mandatory HREK authored-crosshair resolver gate',
    'HREK resolver failure latches the rejected generation',
    'mandatory CHUD hook creation follows the camera hooks',
    'mandatory CHUD hook creation failure rejects the complete transaction',
    'mandatory CHUD target publication',
    'mandatory CHUD hook enable'
)
if (-not $reachChudCoreActive) {
    if ($game -notmatch 'const\s+bool\s+hudDrawWidgetCreated\s*=\s*false' -or
        $game -notmatch 'g_reachCamera\.hudDrawWidgetTarget\s*=\s*nullptr' -or
        $game -match 'MH_EnableHook\(hudDrawWidget\)') {
        throw 'Reach core-only gate rejected: unproven CHUD ownership is not fully disabled.'
    }
}
foreach ($entry in $requiredChudGame.GetEnumerator()) {
    if (-not $reachChudCoreActive -and
        $inactiveChudCoreChecks -contains $entry.Key) {
        continue
    }
    if ($game -notmatch $entry.Value) {
        throw "Reach authored-crosshair parity gate missing: $($entry.Key)."
    }
}

$requiredChudLogic = [ordered]@{
    'official HREK class-2 identity' =
        'kReachChudCrosshairScriptingClass\s*=\s*2\s*;'
    'official optimized HREK entry signature' =
        'kReachHrekChudDrawWidgetAob[\s\S]{0,220}?48 8B C4 44 89 48 20[\s\S]{0,220}?48 81 EC C0 00 00 00'
    'official HREK body-size alternatives' =
        'kReachTagPlayChudDrawWidgetBodySize\s*=\s*0x424[\s\S]{0,360}?kReachSapienPlayChudDrawWidgetBodySize\s*=\s*0x483'
    'tag-play argument-2 descriptor ownership' =
        'kReachTagPlayChudDescriptorMoveOffset[\s\S]{0,1200}?0x4C,\s*0x8B,\s*0xF2'
    'sapien-play argument-2 descriptor ownership' =
        'kReachSapienPlayChudDescriptorMoveOffset[\s\S]{0,1800}?0x4C,\s*0x8B,\s*0xFA'
    'both official variants retain the fifth-argument stack load' =
        'kReachTagPlayChudFifthArgumentLoadOffset[\s\S]{0,1800}?0x4C,\s*0x8B,\s*0x4D,\s*0x7F[\s\S]{0,2200}?kReachSapienPlayChudFifthArgumentLoadOffset[\s\S]{0,1800}?0x4C,\s*0x8B,\s*0x4D,\s*0x7F'
    'both official variants read descriptor plus four' =
        'kReachTagPlayChudClassReadOffset[\s\S]{0,1800}?0x56,\s*0x04,\s*0xE8[\s\S]{0,1800}?kReachSapienPlayChudClassReadOffset[\s\S]{0,1800}?0x57,\s*0x04,\s*0xE8'
    'both official variants retain post-class-call flow' =
        'kReachTagPlayChudClassReadOffset\s*\+\s*10[\s\S]{0,240}?0x48,\s*0x8B,\s*0xD0[\s\S]{0,2200}?kReachSapienPlayChudClassReadOffset\s*\+\s*10[\s\S]{0,240}?0x48,\s*0x8B,\s*0xD0'
    'closed authored-crosshair action set' =
        'enum\s+class\s+ReachChudCrosshairAction[\s\S]{0,220}?\{\s*DrawStock,\s*Suppress,\s*CaptureAuthored,\s*RejectTransaction,\s*\}'
    'non-owned CHUD always fails open before descriptor validation' =
        'if\s*\(\s*!ownsStereoTransaction\s*\)[\s\S]{0,100}?DrawStock[\s\S]{0,120}?if\s*\(\s*!descriptorReadable\s*\)'
    'owned unreadable descriptor rejects the transaction' =
        'if\s*\(\s*!descriptorReadable\s*\)[\s\S]{0,100}?RejectTransaction'
    'readable non-class-2 widget stays stock' =
        'if\s*\(scriptingClass\s*!=\s*kReachChudCrosshairScriptingClass\)[\s\S]{0,100}?DrawStock'
    'owned invalid stereo eyes reject the transaction' =
        'if\s*\(stereoEye\s*<\s*0\s*\|\|\s*stereoEye\s*>\s*1\)[\s\S]{0,100}?RejectTransaction'
    'universal crosshair disable suppresses exact class 2' =
        'if\s*\(\s*!crosshairEnabled\s*\)[\s\S]{0,80}?Suppress'
    'universal native-reticle choice remains intentional stock behavior' =
        'if\s*\(\s*!killNativeReticle\s*\)[\s\S]{0,80}?DrawStock'
    'configured first-eye-only authored capture' =
        'captureEye\s*=\s*rightEyeFirst\s*\?\s*1\s*:\s*0[\s\S]{0,180}?stereoEye\s*==\s*captureEye[\s\S]{0,120}?CaptureAuthored[\s\S]{0,100}?Suppress'
    'closed authored class-2 pair rule' =
        'ReachAuthoredCrosshairPairComplete\([\s\S]{0,360}?return\s*!authoredCrosshairRequired\s*\|\|\s*!class2Seen\s*\|\|[\s\S]{0,80}?authoredCaptureCompleted'
    'closed complete-projection transaction rule' =
        'ReachCanSubmitCompleteProjection\([\s\S]{0,420}?return\s*!reachTitle\s*\|\|\s*\(\s*stereoUploadComplete\s*&&\s*!authoredUploadFailed\s*&&[\s\S]{0,100}?liveReachOwnerAfterUpload\s*\)'
}
foreach ($entry in $requiredChudLogic.GetEnumerator()) {
    if ($chudLogic -notmatch $entry.Value) {
        throw "Reach authored-crosshair parity gate missing: $($entry.Key)."
    }
}

$requiredReticleComposition = [ordered]@{
    'cold authored-reticle preparation creates every required resource' =
        'AuthoredReticlePreparationResult\s+VR_PrepareAuthoredReticleResources\(\)[\s\S]{0,1800}?CreateChain\([\s\S]{0,700}?GetRtv\([\s\S]{0,420}?EnsureAuthoredReticleTexture\(\)'
    'authored-resource startup readiness is acquire/release published' =
        'if\s*\(StartFrameWaitThread\(\)\)[\s\S]{0,420}?g_sessionRunning\s*=\s*true[\s\S]{0,420}?g_authoredReticlePreparationReady\.store\(\s*true,\s*std::memory_order_release\s*\)'
    'authored-resource readiness is consumed with acquire ordering' =
        'VR_CanPrepareAuthoredReticleResources\(\)[\s\S]{0,220}?g_authoredReticlePreparationReady\.load\(\s*std::memory_order_acquire\s*\)'
    'authored-resource readiness is revoked on session stop' =
        'XR_SESSION_STATE_STOPPING[\s\S]{0,520}?g_authoredReticlePreparationReady\.store\(\s*false,\s*std::memory_order_release\s*\)[\s\S]{0,160}?g_sessionRunning\s*=\s*false'
    'authored-resource readiness is revoked on fatal drain' =
        'EnterFrameWaitFatalDrain\(const char\*\s+reason\)[\s\S]{0,520}?g_authoredReticlePreparationReady\.store\(\s*false,\s*std::memory_order_release\s*\)'
    'prepared hot capture only validates prebuilt resources' =
        'if\s*\(requirePreparedResources\)[\s\S]{0,620}?g_reticleChain\s*==\s*XR_NULL_HANDLE[\s\S]{0,300}?g_authoredReticleTexture[\s\S]{0,120}?g_authoredReticleRtv[\s\S]{0,280}?else[\s\S]{0,420}?EnsureReticleChain\(\)[\s\S]{0,120}?EnsureAuthoredReticleTexture\(\)'
    'Reach prepared capture selects the allocation-free branch' =
        'bool\s+VR_BeginPreparedAuthoredReticleCapture\(\)[\s\S]{0,160}?BeginAuthoredReticleCaptureInternal\(true\)'
    'Reach attempt invalidation clears prior ready serial without touching legacy callers' =
        'void\s+VR_InvalidatePreparedReachAuthoredReticleCapture\(\)[\s\S]{0,420}?g_authoredReticleReady\s*=\s*false[\s\S]{0,120}?g_authoredReticleSerial\s*=\s*0'
    'Reach prepared capture end suppresses hot-hook logging' =
        'bool\s+VR_EndPreparedAuthoredReticleCapture\(\)[\s\S]{0,160}?return\s+EndAuthoredReticleCaptureInternal\(false\)'
    'frame-serial-bound authored-reticle admission' =
        'const\s+bool\s+authoredReticleThisFrame\s*=\s*g_authoredReticleReady\s*&&\s*g_authoredReticleSerial\s*==\s*g_preparedFrame\.serial\s*;'
    'Reach authored composition requires narrow active ownership' =
        'const\s+bool\s+reachAuthoredReticleThisFrame\s*=\s*reachTitle\s*&&\s*reachImages\s*&&\s*authoredReticleThisFrame\s*&&\s*Game_OwnsReachAuthoredReticle\(\)\s*;'
    'Reach title can never enter reticle composition through ControllerAim alone' =
        'const\s+bool\s+reticleOwnerAdmitted\s*=\s*reachTitle\s*\?\s*reachReticleReadyForSubmit\s*:\s*Game_HasTitleCapability\(\s*TitleCapability_ControllerAim\s*\)\s*;'
    'non-Reach shared upload preserves chain-first admission' =
        'nonReachReticleUploadAdmitted\s*=\s*!reachTitle[\s\S]{0,260}?TitleCapability_ControllerAim[\s\S]{0,180}?g_config\.crosshair\s*&&\s*haveAim\s*&&\s*EnsureReticleChain\(\)\s*;'
    'Reach upload requires its complete authored eye-owner conjunction' =
        'shouldUploadAuthoredReticle\s*=\s*reachTitle\s*\?\s*reachAuthoredReticleThisFrame\s*:\s*authoredReticleThisFrame\s*&&\s*nonReachReticleUploadAdmitted\s*;'
    'Reach stereo upload requires both successful eye resolves' =
        'everyReachEyeUploaded\s*=\s*reachImages[\s\S]{0,900}?eyeUploaded\s*=\s*BlitImageQuality\([\s\S]{0,320}?everyReachEyeUploaded\s*&&\s*eyeUploaded'
    'Reach swapchain completion excludes timeout-success codes' =
        'RequireReachSwapchainCompletion\([\s\S]{0,620}?if\s*\(result\s*==\s*XR_SUCCESS\)[\s\S]{0,260}?g_abortFrameForReachSwapchainFailure\s*=\s*true[\s\S]{0,180}?EnterFrameWaitFatalDrain\(failureReason\)'
    'fresh OpenXR session clears terminal Reach swapchain abort state' =
        'StartFrameWaitThread\(\)[\s\S]{0,900}?g_waitPipelineFaulted\.store\(\s*false,[\s\S]{0,120}?g_abortFrameForReachSwapchainFailure\s*=\s*false'
    'Reach world acquire requires exact XR_SUCCESS' =
        'stereoAcquired\s*=\s*reachTitle\s*\?\s*stereoAcquire\s*==\s*XR_SUCCESS\s*:\s*XR_SUCCEEDED\(stereoAcquire\)'
    'Reach world wait uses exact completion predicate' =
        'stereoWait\s*=\s*xrWaitSwapchainImage\([\s\S]{0,260}?reachTitle\s*\?\s*RequireReachSwapchainCompletion\(\s*stereoWait'
    'Reach stereo upload requires XR image release' =
        'stereoRelease\s*=\s*xrReleaseSwapchainImage[\s\S]{0,260}?reachTitle\s*\?\s*RequireReachSwapchainCompletion\([\s\S]{0,180}?stereoRelease[\s\S]{0,260}?reachStereoUploadComplete\s*=[\s\S]{0,120}?everyReachEyeUploaded\s*&&\s*stereoReleased'
    'failed Reach swapchain transaction closes begun frame without layers' =
        'if\s*\(g_abortFrameForReachSwapchainFailure\)[\s\S]{0,520}?backbuffer->Release\(\)[\s\S]{0,220}?EndPreparedFrameWithoutLayers\([\s\S]{0,160}?return[\s\S]{0,520}?if\s*\(Menu_IsOpen\(\)\)'
    'Reach stereo upload or projection-view failure revokes the pair' =
        'reachTitle\s*&&\s*reachImages\s*&&[\s\S]{0,160}?!reachStereoUploadComplete\s*\|\|\s*projection\.viewCount\s*!=\s*2[\s\S]{0,420}?g_reachEyeSerial\[0\]\.store[\s\S]{0,220}?g_reachEyeSerial\[1\]\.store[\s\S]{0,320}?Game_RejectReachAuthoredReticle\(reachGeneration\)'
    'Reach projection images require completed stereo upload' =
        'projectionImagesReady\s*=\s*reachTitle\s*\?\s*reachImages\s*&&\s*reachStereoUploadComplete\s*:\s*g_eyeHasImage\[0\]\s*&&\s*g_eyeHasImage\[1\]'
    'admitted current-frame authored upload remains mandatory' =
        'if\s*\(shouldUploadAuthoredReticle\s*&&\s*!UploadAuthoredReticle\(reachTitle\)\)'
    'Reach authored acquire requires exact XR_SUCCESS and terminal nonexact success handling' =
        'if\s*\(requireSuccessfulRelease\s*&&\s*acquireResult\s*!=\s*XR_SUCCESS\)[\s\S]{0,260}?XR_SUCCEEDED\(acquireResult\)[\s\S]{0,220}?RequireReachSwapchainCompletion\(\s*acquireResult'
    'Reach authored upload requires successful swapchain release' =
        'UploadAuthoredReticle\(bool\s+requireSuccessfulRelease\)[\s\S]{0,1800}?RequireReachSwapchainCompletion\([\s\S]{0,220}?waitResult[\s\S]{0,900}?releaseResult\s*=\s*xrReleaseSwapchainImage[\s\S]{0,300}?RequireReachSwapchainCompletion\([\s\S]{0,180}?releaseResult'
    'Reach authored upload failure rejects active ownership' =
        'authoredUploadFailed\s*=\s*true[\s\S]{0,220}?if\s*\(reachTitle\)[\s\S]{0,900}?Game_RejectReachAuthoredReticle\(\s*reachGeneration\s*\)'
    'Reach upload failure never enters legacy swapchain maintenance' =
        'if\s*\(reachTitle\)[\s\S]{0,900}?Game_RejectReachAuthoredReticle\(\s*reachGeneration\s*\)\s*;[\s\S]{0,100}?else\s*EnsureReticleChain\(\)'
    'Reach upload failure revokes both copied eye serials' =
        'if\s*\(reachTitle\)[\s\S]{0,520}?g_reachEyeSerial\[0\]\.store\(\s*0,[\s\S]{0,160}?g_reachEyeSerial\[1\]\.store\(\s*0,[\s\S]{0,220}?Game_RejectReachAuthoredReticle'
    'Reach rechecks exact ownership after authored upload' =
        'liveReachOwnerAfterUpload\s*=\s*!reachTitle\s*\|\|\s*Game_OwnsReachAuthoredReticle\(\)\s*;'
    'post-upload ownership loss revokes eye pair and ages Reach art' =
        'if\s*\(reachTitle\s*&&\s*!liveReachOwnerAfterUpload\)[\s\S]{0,700}?g_reachEyeSerial\[0\]\.store\(\s*0,[\s\S]{0,180}?g_reachEyeSerial\[1\]\.store\(\s*0,[\s\S]{0,320}?g_authoredReticleReady\s*=\s*false\s*;[\s\S]{0,120}?g_authoredReticleSerial\s*=\s*0\s*;'
    'Reach complete projection admission consumes upload and owner result' =
        'reachProjectionAdmitted\s*=\s*ReachCanSubmitCompleteProjection\(\s*reachTitle,\s*reachStereoUploadComplete,\s*authoredUploadFailed,\s*liveReachOwnerAfterUpload\s*\)\s*;'
    'Reach submitted reticle never enters lazy chain preparation' =
        'reticleChainAdmitted\s*=\s*reachTitle\s*\?\s*reachReticleReadyForSubmit\s*:\s*nonReachReticleUploadAdmitted'
    'Reach projection is queued only through complete admission' =
        'if\s*\(reachProjectionAdmitted\)[\s\S]{0,240}?layers\.push_back\([\s\S]{0,180}?&projection'
    'Reach authored quad shares complete projection admission' =
        'reachReticleReadyForSubmit\s*=\s*reachProjectionAdmitted\s*&&\s*reachAuthoredReticleThisFrame\s*&&\s*!authoredUploadFailed\s*;'
    'Reach scope layer shares complete projection admission' =
        'if\s*\(reachProjectionAdmitted\s*&&\s*Game_AllowsSharedGameplayFeatures\(\)'
    'procedural opacity remains camera-only and never Reach-specific' =
        'kProceduralOpacity\s*=\s*Game_IsCameraOnlyBringup\(\)\s*\?\s*1\.0f\s*:\s*0\.0f'
}
foreach ($entry in $requiredReticleComposition.GetEnumerator()) {
    if ($vr -notmatch $entry.Value) {
        throw "Reach authored-crosshair parity gate missing: $($entry.Key)."
    }
}

$reachCapabilities = [regex]::Match(
    $titleRegistry,
    'constexpr\s+uint32_t\s+kReachCapabilities\s*=\s*(?<body>[\s\S]*?);')
if (!$reachCapabilities.Success -or
    $reachCapabilities.Groups['body'].Value -match 'TitleCapability_Hud') {
    throw 'Reach authored-crosshair parity gate rejected: native HUD capability must remain withheld.'
}

$cameraOnlyBringup = [regex]::Match(
    $game,
    'bool\s+Game_IsCameraOnlyBringup\(\)\s*\{(?<body>[\s\S]*?)\n\}')
if (!$cameraOnlyBringup.Success -or
    $cameraOnlyBringup.Groups['body'].Value -notmatch 'OdstCameraOnlyContext|return\s+false' -or
    $cameraOnlyBringup.Groups['body'].Value -match 'HaloReach|g_reach') {
    throw 'Reach authored-crosshair parity gate rejected: procedural opacity admission must remain ODST-only.'
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

Write-Host 'Reach core Halo 3/ODST parity gate passed: five camera/FP hooks, native weapon-IK bypass, world projection, and the unproven CHUD hook disabled.'
