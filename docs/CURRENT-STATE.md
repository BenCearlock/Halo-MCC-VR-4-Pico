# Current state

Authoritative as of 2026-07-25. This file is the only active accepted-build
pointer. Detailed pre-cleanup experiments remain available in Git history; they
are evidence, not instructions.

## Newest headset-accepted private cumulative source

The current development pointer is commit
`a5524d3fe58e4ed5507c27429ccca52a3d4fdf7d` on `reach/campaign-parity`.
It descends from the accepted 0.2.2 runtime source and is an accepted private
milestone, not a public release or tag. The public known-good product remains
`MCC_VR_ALPHA_0.2.2`.

| Identity | Value |
| --- | --- |
| Headset-tested runtime source | `a5524d3fe58e4ed5507c27429ccca52a3d4fdf7d` |
| Build | Release x64, preset `release-reach-private`, ODST ON, Reach ON |
| Candidate package | `out/candidates/a5524d3-reach-private-20260724-023052584Z` |
| `halo3xr.dll` SHA-256 | `2BD8C0A8675C393715AD52F29301984B1A57CE45B5340070713F153E2CADE2A2` |
| `halo3xr_launcher.exe` SHA-256 | `ED0540A7A6F758543F1E828E73C35435D0CA092D259BAE285472804276F8A441` |
| Reach milestone | Shared virtual-controller transport only; Reach runtime hooks remain OFF |
| Preserved test evidence | `out/test-runs/a5524d3-reach-h3-odst-headset-pass-20260724-023358Z` |

### Reach controller and cross-title headset confirmation - 2026-07-23

- The installed DLL and launcher were hashed separately after the manual copy
  and matched the candidate manifest exactly.
- The first runtime-log line reported source
  `a5524d3fe58e4ed5507c27429ccca52a3d4fdf7d`, ODST ON, Reach ON, compiled
  `Jul 23 2026 21:30:25`.
- Title coverage in one MCC session was Reach, Halo 3, ODST, then Reach again.
  The user confirmed Reach worked without breaking Halo 3 or ODST.
- Reach was detected with controller-only admission. Its XInput
  `reads`/`padValid`/`merged` counters continued rising while stereo remained
  off, as required for this milestone.
- Halo 3 subsequently armed stereo and its accepted first-person path. ODST
  then completed preflight, armed stereo/6DOF, exercised native pause teardown,
  and safely returned ownership before Reach regained controller transport.
- This result does not authorize or claim Reach camera, stereo, 6DOF, aim,
  movement, HUD, IK, haptics, or lifecycle hooks.

### Partial/failed Reach floating-hands / FP-camera result - 2026-07-25

Candidate `a1dcb7beeb0bec56b3b7c04a6f15a897eaa63fa4` combined the
Reach-only forced-floating-hands palette policy with the first Reach
first-person camera-rebuild detour. Its exact packaged and deployed identity
was:

| Identity | Value |
| --- | --- |
| Candidate package | `out/candidates/a1dcb7b-reach-fp-parity-20260725-085113864Z` |
| `halo3xr.dll` SHA-256 | `68793B3052EE2AE60F197526A7A215913FA7EEEFC3BB4177A613EE4AAFFF70A0` |
| `halo3xr_launcher.exe` SHA-256 | `F9C3778B5AD993E26039BA731698A3E2BCD10159F00F6C31C15C5E450DE516FB` |
| Headset result | Hands were a little better, but the gun/both-hands FOV, projection, apparent location, and tracking-distance coverage were incorrect |

The runtime log proved that the Reach FP-camera hook installed and the camera
core armed. It also proved that the private-palette path executed by reporting
`Reach FP forced floating-hands active`. It did **not** prove that the
post-original world-camera substitution executed.

Pinned retail and HREK static evidence now explains that missing proof. Every
visible FP wrapper copies the outer camera into, then pushes, a dedicated nested
camera-stack workspace at `haloreach+0x00CFAC20`. That workspace holds the
compact camera at `+0`, the derived/projection block at `+0x1E4`, and callback
`haloreach+0x0000C380` at `+0x2A8`; the wrappers pass the exact FP view at
`haloreach+0x00BB8F68` to rebuild `0x00286C6C` before popping the nested
workspace. The candidate instead required the live stack top to equal the outer
default workspace `0x00C9FAE0` and selected that outer workspace's `+0x1E4`.
That guard is impossible during a normal visible FP rebuild, so the intended
camera swap returned before its copies and uploader call.

The corrected nested-workspace candidate is pending. `a1dcb7b` is a
partial/failed headset result and does **not** advance the accepted pointer;
that pointer remains `a5524d3fe58e4ed5507c27429ccca52a3d4fdf7d`.

### Unaccepted Reach nested FP projection / residual ownership result - 2026-07-25

Candidate `fe0c48e4282fc17d05ca2de0d8dcaa36c95483dd` corrected the
first-person camera destination to the exact nested workspace. Its packaged and
separately verified deployed identity was:

| Identity | Value |
| --- | --- |
| Candidate package | `out/candidates/fe0c48e-reach-fp-parity-20260725-094807875Z` |
| `halo3xr.dll` SHA-256 | `5D1767B492A5369EABAD312B10F5528588D76B2AD4B647BCB64284717F310E1A` |
| `halo3xr_launcher.exe` SHA-256 | `0758315C8091A46E7F1798FE1B1D7F1E704A61B9BF43CF799467A1A702F5E5E4` |
| Headset result | The major FP presentation improved, leaving a small left-hand fragment/ribbon following the right controller and a small hand/gun offset during physical head turns |

The runtime first line matched the exact source. It reported both-eye nested
world-camera uploads, `Reach FP forced floating-hands active: body=47 live=52`,
and the native weapon-IK bypass. The remaining fragment is therefore downstream
palette ownership, not a failed projection swap or native IK overwrite.

Official HREK exports identify the exact leak. Spartan glove vertices blend
`l_hand` with its parent `l_forearm`, while every `flair_forearm` permutation is
rigid on `l_radius`. Candidate `754b34b` therefore tested moving the complete
hidden left influence branch with the controller while retaining hand-only
visibility. Its exact headset failure below proves that ownership alone is not
enough when those hidden records are still collapsed at separate joint pivots.

The head-turn offset is a separate Reach-only defect: prepared wrist targets
are already absolute `gameplayBase + tracked room offset`, but the rejected
diagnostic-era render-root rebase adds tracked head translation again at both
marker and palette consumption. Its removal remains a separate headset
candidate so the two behavioral corrections are not stacked. `fe0c48e` remains
unaccepted and does not advance the pointer above.

### Failed Reach left-branch ownership / collapse-pivot result - 2026-07-25

Candidate `754b34b2acfdae81c6dbd833d3b7bd7b0e1e7b3d` moved the exact
HREK-proven left influence closure with the left controller, then retained the
existing hand-only visibility mask. Its exact packaged and deployed identity
was:

| Identity | Value |
| --- | --- |
| Candidate package | `out/candidates/754b34b-reach-fp-parity-20260725-115117345Z` |
| `halo3xr.dll` SHA-256 | `BE95F23246B5AAAD1C0C492C7C4660D3EEDB075D0A610B47BF924129C899EDD0` |
| `halo3xr_launcher.exe` SHA-256 | `3A8C3216B64C910E6EB60029D3DA63CA9305BD857297C47F227B7E7163CABFE0` |
| Headset result | Slightly better, but a severe black wrist/forearm ribbon still stretched from the independently tracked left hand; gun and both hands still drifted slightly with physical head turns |

The installed hashes and first log line matched this exact candidate. Reach
reported the Spartan 47-over-52 private palette, native weapon-IK bypass, and
both-eye nested world FP projection active with no palette validation,
finite-value, or frame-order warning. The preserved log is
`out/test-runs/754b34b-reach-wrist-collapse-fail-20260725-070700Z/halo3xr.log`,
SHA-256 `ED23B61BE925F78FB6C52B3A7258808EE66B41B5FBA20B4E79A5B012A9D8F9C5`.

The official HREK mesh exporter resolves the remaining artifact. In both
Spartan arms meshes, all 103 nondegenerate left `spartan_rubber_suit` triangles
span two to four of `l_upperarm`, `l_forearm`, `l_humerus`, and `l_radius`.
The glove independently has 58 `l_forearm`/`l_hand` cross-weight vertices over
96 triangles. The failed candidate moved those records together, but the final
floating-hands pass still assigned scale `0.0001` at four distinct arm-joint
translations. Linear skinning therefore drew the photographed strip between
those collapsed pivots. Elite independently has 51 cross-weight vertices over
83 hand/forearm triangles plus 96 body triangles spanning multiple auxiliary
bones. The replacement candidate collapses all four hidden auxiliary records at
the final solved left-wrist record, retaining the hand-only visible mask.

The head-turn drift remains the separate prepared-target rebase defect described
above and is not stacked into this wrist candidate. `754b34b` is failed and does
not advance the accepted pointer.

### Partial Reach left-wrist anchor pass / residual right-wrist result - 2026-07-25

Candidate `765604cc631a1c0042a468738c7545ffbbd9208a` replaced the failed
left-branch behavior by co-locating all four hidden left-arm skin influences at
the solved left wrist before their tiny collapse. Its exact packaged and
deployed identity was:

| Identity | Value |
| --- | --- |
| Candidate package | `out/candidates/765604c-reach-fp-parity-20260725-121927950Z` |
| `halo3xr.dll` SHA-256 | `196BE16C85D2837DA9E8822FC896122F6CFC47653196FD020F9D0109D2F804AE` |
| `halo3xr_launcher.exe` SHA-256 | `C09CC08C8B6463A356BE05567374EBDB0426F2F1379B2ADAB59CB93AB2671C2B` |
| Headset result | The left-hand ribbon was fixed, while the equivalent severe stretched strip remained at the right/weapon wrist |

The installed hashes and first log line matched this exact source. Reach again
reported the Spartan 47-over-52 private palette, exact native weapon-IK bypass,
and both-eye nested world FP projection, with zero frame-order failures and
clean teardown. The preserved log is
`out/test-runs/765604c-reach-left-pass-right-wrist-fail-20260725-122522Z/halo3xr.log`,
SHA-256 `7E18D3D53127A1B5235B99C4395E01DA9C8296CDE5CFCE55B5B228F02D730F57`.

The same official HREK exports independently resolve the right artifact.
Spartan has 59 `r_forearm`/`r_hand` cross-weight vertices over 96 triangles and
71 right `spartan_rubber_suit` vertices over 103 multi-auxiliary triangles.
Elite has 51 cross-weight vertices over 83 triangles and 75 right body vertices
over 99 multi-auxiliary triangles. Those right auxiliary output nodes map in
both official graphs to sources `{6,9,10,14}`
(`r_upperarm/r_forearm/r_humerus/r_radius`), exact mask `0x4640`; the solved
right wrist is source 13. Appended held objects begin only after body prefix 47
for Spartan or 41 for Elite, so this mask cannot touch the weapon. The forward
candidate retains the headset-confirmed left anchor and applies the identical
wrist-local collapse only to this independently proven hidden right set.

The physical-head-turn drift remains a separate prepared-target rebase defect
and is still not stacked into the wrist work. `765604c` is a narrow left-wrist
headset pass but not a cumulative accepted candidate; the accepted pointer
remains `a5524d3fe58e4ed5507c27429ccca52a3d4fdf7d`.

### Failed Reach final-palette-only candidate - 2026-07-24

Candidate `abea61f0daf2b70ba779a40a3a2ad72b3debf121` implemented the
final-palette reconstruction architecture described below. Retail Reach builder
`0x2AF648` makes separate interpolation then
palette submissions at `0x2AF85A -> 0x2B52EC` and
`0x2AF8F6 -> 0x2B52EC`. Official HREK tags independently identify the two
visible consumers: `objects\characters\spartans\fp\fp.render_model` is the
47-node first-person arms model, while
`objects\characters\spartans\fp_body\fp_body.render_model` is the separate
82-node first-person body model. Treating only the 47-node submission as visible
was not parity and is rejected.

The exact 47/41-node Spartan/Elite map still performs stock-only layout
discovery. On a later pair, that verified animation layout is selected by title
generation plus interpolation view/id/slot and full live count, not by a
temporary source-buffer address. Each retail interpolation transaction receives
its own bounded context. Each final palette consumes the newest exact
source-pointer match and reconstructs the complete live source (through the
retail 120-node bound) from its untouched snapshot into private scratch before
calling the stock palette builder. The 47-node arms palette and the 82-node body
palette therefore receive the same right- and left-wrist solution. The exact
appended held-object range inherits the solved right-wrist delta inside that
same reconstruction.

As in Halo 3/ODST, the mutated live interpolation graph exists only for
marker/muzzle/attachment consumers and receives one rigid right-controller
transform. No visible palette consumes it. The failed Reach-only separated-hand
live-graph owner, body-only admission gate, and source-pointer-keyed layout cache
have been removed; they are not dormant fallbacks. Unknown or invalid layouts
remain wholly stock rather than partially modified.

The installed DLL SHA-256 was
`61C70876A8BC883D5277A7070EF38E2CB350476B6BFAFD1943B96C6EF67ADF91`.
The runtime first line matched source `abea61f0...`; Reach armed and reported
`body=47 live=52 arm_ik=1 floating_hands=0`. The headset result was still no
visible change: the forearm moved while the left hand remained attached to the
gun/right hand. Evidence is preserved under
`out/test-runs/abea61f-reach-fp-parity-no-visible-change-20260725-052246Z`.
This candidate is failed and does **not** advance the accepted pointer.

### Failed Reach native weapon-IK parity gate / no-3D result - 2026-07-25

The missing accepted Halo 3/ODST behavior is now identified exactly: after the
palette transaction, both accepted titles bypass native flat-screen weapon IK
so its support-hand solve cannot reattach the controller-owned hand to the gun.
The final-palette candidate omitted that stage.

Candidate `cd0a7c136caa2972d9d57f5e44929adb88b96069` added that
title-native bypass, but its runtime proof incorrectly required retail's debug
descriptor to publish the HREK development value pointer. Retail leaves that
descriptor field unpublished. The exact installed DLL SHA-256 was
`ECA7202AE4132AC18A6E8C403C0FE4322616E380FC098D05E4B16135FB25D174`.
Cold preflight and display-worker proof passed, then the native weapon-IK proof
failed open before Reach installed any camera hooks. The headset therefore
showed stock flat-screen output with no 3D. Evidence is preserved under
`out/test-runs/cd0a7c1-reach-no-3d-weapon-ik-proof-fail-20260725-053853Z`.
This candidate is failed and does **not** advance the accepted pointer.

Pinned HREK exposes the type-5 boolean
`debug_animation_fp_weapon_ik_disable`. Its development table entry at
`0x201AD98` publishes value pointer `0x4F40A60`; the post-palette homolog
compares that byte at `0x8D3162` and jumps to its existing no-weapon-IK
epilogue at `0x8D338B`. Pinned retail Reach repeats the same execution edge but
does not publish the value pointer in its descriptor: the unique decision AOB
at `0x2B506E` directly compares exact byte `0x4E38B61` at `0x2B507F`, and
`0x2B5085` jumps to epilogue `0x2B52D1` when it is nonzero.

Forward source requires the exact retail descriptor name/type identity, unique
retail AOB, decoded RIP-relative value target, decoded stock branch target, and
bounded boolean value. It binds the control from that pinned shipping consumer
instruction, sets it for the complete Reach VR transaction, and restores its
original value during verified teardown. There is no runtime probe, skeleton
guess, alternate owner, or fallback implementation. Headset acceptance is
pending.

The forward-corrected candidate `d721068ac6ace2f2d2b6c8107c2f3d18494e43bd`
is installed with DLL SHA-256
`46ABD7BFF4AAF083A8CEF6AB174831458A06FAE7EBA85815D327ACECE2ABF226`.
The installed hash was verified separately, `arm_ik=1` and `floating_hands=0`
remain unchanged, MCC was not launched, and headset acceptance is pending.

### Failed Reach separated-hand graph result - 2026-07-24

This result is evidence only and does not advance the accepted pointer above.

| Identity | Value |
| --- | --- |
| Tested runtime source | `6e31751cce1dd78a315f5418be8d7d2736e1f2a9` |
| Candidate package | `out/candidates/6e31751-reach-two-arm-ik-20260725-042432527Z` |
| `halo3xr.dll` SHA-256 | `49DC585C1E57FB54D197198E2A46AE95AA1CBF0FEE565EDCD62BBE740B9E8715` |
| Headset result | Nothing changed: the left forearm moved, but the visible left hand remained stuck to the gun/right-hand assembly |

The installed hash and first log line matched this exact clean source, and the
log reported the Reach two-arm path active with a 47-node palette over a
52-node live graph. This rejects the separated source-owner change. Combined
with retail `0x2AF648` and the official HREK 47-node arms / 82-node body split,
the result identifies the architectural fault: `6e31751` admitted and solved
only the 47-node palette transaction while allowing the second visible body
palette to consume stock/live data. Per the user's explicit instruction, this
failed DLL is not rolled back in the MCC installation; it is simply not an
accepted pointer or basis for the forward implementation.

### Failed Reach whole-graph hand-parenting result - 2026-07-24

This result is evidence only and does not advance the accepted pointer above.

| Identity | Value |
| --- | --- |
| Tested runtime source | `e08b538f715a01d868a9460f05afae6a0cc0410e-dirty` |
| `halo3xr.dll` SHA-256 | `236D06940F47E38876A54DF4AFE07E4C484AED2E025865AF55121E022E1772AC` |
| Preserved failure evidence | `out/test-runs/e08b538-dirty-reach-hands-parented-20260725-040508Z` |
| Headset result | The left forearm moved, but the left hand remained stuck to the gun/right-hand assembly instead of tracking independently |

The runtime proved both OpenXR aim poses valid and tracked, put the left target
on the correct side, and reported both exact 47-node arms-palette wrists at
their targets with zero error. The live graph also ran. Controller tracking,
snapshot publication, and the analytic wrist solve are therefore ruled out.
The later `6e31751` separated-owner result also produced no visible change, so
both live-graph ownership models are rejected. The forward path no longer uses
either model for visible geometry; it reconstructs every final palette from its
own untouched interpolation transaction, matching Halo 3/ODST.

### Failed Reach two-arm scope-timing result - 2026-07-24

This result is evidence only and does not advance the accepted pointer above.

| Identity | Value |
| --- | --- |
| Tested runtime source | `7ea6aca845a698f7994ef355e76a7361fe6f154e` |
| Candidate package | `out/candidates/7ea6aca-reach-two-arm-ik-20260724-214242079Z` |
| `halo3xr.dll` SHA-256 | `61C1DAECF3EE4D8891C88F4C973655ACDA949F61B938A865CDB628D7FB225F81` |
| Preserved failure evidence | `out/test-runs/7ea6aca-reach-ik-no-layout-20260725-001238Z` |
| Headset result | Reach camera/stereo and controller aim armed, but no arm IK appeared |

The exact installed DLL and configuration (`arm_ik=1`, `floating_hands=0`)
were correct. The runtime log contained no layout-learned, two-arm-active, or
layout-rejected status. Static retail call edges explain that exact absence:
`main_render_view` performs FP preparation through
`0x256724 -> 0x264530 -> 0x2AF648`, including interpolation at `0x2AF85A`,
before it calls `player_view_render` at `0x0C33C4`. Source `7ea6aca` armed the FP
scope only in the later inner stereo transaction, so every interpolation
callback failed the scope guard and every palette remained stock. The forward
candidate moves scope ownership to the admitted outer boundary, preserves it
through both eyes, and keeps nested renders stock while restoring the bounded
parent live graph and context. No additional probe is required.

### Unaccepted Reach camera headset result - 2026-07-24

This result is evidence only and does not advance the accepted pointer above.

| Identity | Value |
| --- | --- |
| Tested runtime source | `0f7b6321ddc1830ad8a95c2bca8e472e3d837fff` |
| Candidate package | `out/candidates/0f7b632-reach-camera-20260724-093221973Z` |
| `halo3xr.dll` SHA-256 | `C5A33D3994695334CBB2F8DD0F108A42B1A86DF6BB3B2A2F646EF6A89EE01C40` |
| Preserved failure evidence | `out/test-runs/0f7b632-reach-3d-warp-input-fail-20260724-094945Z` |
| Headset result | Distinct 3D and translation reached the headset; projection warped on head turns, black outer borders remained, and stock look competed with HMD look |

The installed artifact matched its manifest and the runtime log proved both
Reach eye copies were current. The failure was traced to a concrete view
contract mismatch: Reach rastered approximately 61.5/53-degree horizontal/
vertical half-FOV at `2912x2100`, but OpenXR received Halo 3's approximately
47.5/48.1-degree defaults. The next forward candidate binds the actual Reach
projection and eye copies to the same prepared frame and gives the armed tracked
camera exclusive visual look-stick ownership. It remains headset-pending and
does not claim Reach weapon/body aim, HUD, or arm IK.

### Unaccepted Reach stereo-pass / culling-fail result - 2026-07-24

This result is evidence only and does not advance the accepted pointer above.

| Identity | Value |
| --- | --- |
| Tested runtime source | `f953bbe373df22dbbd4b41c344c1226b738260ba` |
| Candidate package | `out/candidates/f953bbe-reach-camera-20260724-102013546Z` |
| `halo3xr.dll` SHA-256 | `2B492F23ECF7CBB158B5EE4072B01CDE1F4BF7439C8BFF8886649547269AC980` |
| `halo3xr_launcher.exe` SHA-256 | `B5E5D136D8283B3B8AE5864AC6EC43FB65D99DC987F64C0D6030A86425F29DDE` |
| Preserved failure evidence | `out/test-runs/f953bbe-reach-stereo-pass-culling-fail-20260724-102643Z` |
| Headset result | The Reach 3D looked great, but world visibility/culling followed the gun/stock aim camera instead of the headset |

The exact installed DLL matched the candidate manifest. The runtime armed
Reach stereo/6DOF, sustained approximately 100 FPS, submitted an OpenXR
projection layer, and reported zero frame-order failures. Retail and pinned
HREK evidence then isolated the remaining ordering defect: `main_render_view`
computes visibility from the secondary workspace camera at `+0x154/+0x1E4`
before the inner `player_view_render` hook installs either HMD eye. The next
forward candidate builds and mirrors one head-centred binocular-union camera at
the exact normal outer boundary before visibility. Its union covers the actual
widened symmetric image each canted eye rasterizes, and the bounded player-view
state receives the same centre for coherent fallback. Both eyes then derive from
that centre without applying turn, head pose, or lean twice. Head/pad/eye data is
one lock-free exact-frame snapshot, and title teardown proves callback/relay
quiescence before releasing hooks or the retained Reach module.
The stock pre-head direction remains separate and is not claimed as Reach
projectile or controller aim.

### Unaccepted Reach head-cull black-screen result - 2026-07-24

This result is evidence only and does not advance the accepted pointer above.

| Identity | Value |
| --- | --- |
| Tested runtime source | `065f62a05a5b7ed4d733ac2ebfd30b5093190c73` |
| Candidate package | `out/candidates/065f62a-reach-camera-20260724-113854197Z` |
| `halo3xr.dll` SHA-256 | `12C6E10BD94B4022A57A697F5A9632786E8FF95AFEB0D139DCE632656038031C` |
| Preserved failure evidence | `out/test-runs/065f62a-reach-head-cull-black-20260724-114200Z` |
| Headset result | Reach became black immediately after stereo armed; the focused OpenXR session submitted zero layers |

The exact installed hash matched the candidate. A fresh Reach re-entry
reproduced `stereo on` followed by `focused shouldRender=1 layers=0`, with no
OpenXR frame-order or display-resource failure. The pinned retail image proves
the normal camera stack is empty at depth `-1`, and its push changes `-1` to
slot/depth `0`. Source `065f62a` incorrectly rejected every negative pre-push
depth in both its outer and propagated inner gates, so neither eye render/copy
could run. The forward correction changes only that proven admission bound to
`-1..2`, retains exact current-depth `pre+1` within `0..3`, and preserves the
head-owned visibility work. It remains headset-pending.

### Unaccepted Reach camera/culling pass, temporal-fog fail - 2026-07-24

This result is evidence only and does not advance the accepted pointer above.

| Identity | Value |
| --- | --- |
| Tested runtime source | `86864bd088867a8e67950eb7d013d1c29d9f2d45` |
| Candidate package | `out/candidates/86864bd-reach-camera-20260724-115400094Z` |
| `halo3xr.dll` SHA-256 | `E66598671EBB602BF5D5B46CAA45F3E0678073603E8150C3A2641723B5DFD209` |
| `halo3xr_launcher.exe` SHA-256 | `4FAA18942886540FD4D212608D2485F17A7D68E575327CA6BF31D0252562ADAC` |
| Preserved headset evidence | `out/test-runs/86864bd-reach-camera-pass-fog-eye-fail-20260724-120101Z` |
| Headset result | Stereo, projection, 6DOF, head-owned visibility, and stick/head coherence looked great; fog/haze appeared eye-swapped and followed head motion |

The installed hashes matched the package. Reach submitted a projection layer at
approximately 94-120 FPS with zero frame-order failures, and the user confirmed
that the earlier gun-owned culling defect was gone. The full log SHA-256 is
`DFB8588BC7808C1902B97C219281AD3CE6B88C6479206EBD3A04973F61E9488F`.
The user also noted somewhat high VRAM use; the exact run allocated a bounded
approximately 395.5 MiB of logical mod/OpenXR texture payload at the runtime's
recommended `3400x3468` eye size, with no per-frame allocation or leak evidence.

Unlike accepted Halo 3 and ODST, Reach still ran its native temporal motion blur.
Pinned retail and HREK evidence identifies unique type-6 float controls
`motion_blur_scale` and `motion_blur_max`, authored as `0.35` and `0.08`.
This led to the first title-native suppression candidate below. Camera, culling,
eye order, projection, and capture remained unchanged.

### Unaccepted Reach invalid-distortion-constants result - 2026-07-24

This result is evidence only and does not advance the accepted pointer above.

| Identity | Value |
| --- | --- |
| Tested runtime source | `facf6b0713ace0432e709916184d938fc553f4b1` |
| Candidate package | `out/candidates/facf6b0-reach-camera-20260724-122131028Z` |
| `halo3xr.dll` SHA-256 | `38BEEF66535A01E0AAC76A6FCFA52117183EEE305F2512663654DB29D6C492A0` |
| `halo3xr_launcher.exe` SHA-256 | `09E1F0450F2C667E43FF8E63F56CC8B08FA34BF9E458DB85DD64F5DA1D6EB5E7` |
| Preserved headset evidence | `out/test-runs/facf6b0-reach-alpha-fog-fail-20260724-072511Z` |
| Headset result | The fog-like contribution remained as a translucent/alpha texture following the head; this was not a valid blur-off result because both distortion operands were zeroed |

The installed hashes and the source identity in the first log line matched the
package. The full preserved log SHA-256 is
`380697D91F174E82B944211D515119A4C76058C7E1FFE07DD86AC4EF2C3854F3`.
The exact retail `apply_distortions` constant builder divides
`motion_blur_max / motion_blur_scale` at `0x00287561`, then divides the scaled
maximum by twice the scale at `0x002875AD`; HREK independently performs the same
operations at `0x0086BBA9` and `0x0086BBF9`. Source `facf6b0` wrote both
authored controls to zero, so both ratios became `0/0` NaNs inside the
screen-space distortion pass. Source `03f0bff` therefore preserved and
reasserted the positive authored scale and zeroed only the maximum. Its exact
headset result below proved that finite policy was active but also proved the fog
artifact was not native motion blur.

### Unaccepted Reach finite-blur-controls / opposite-head fog result - 2026-07-24

This result is evidence only and does not advance the accepted pointer above.

| Identity | Value |
| --- | --- |
| Tested runtime source | `03f0bffbec5a4bdbe0b0784b47aeafc581505f1b` |
| Candidate package | `out/candidates/03f0bff-reach-camera-20260724-124956918Z` |
| `halo3xr.dll` SHA-256 | `456584DF50DF7B7941008BCF23EBC488F24938EE3D2C5B2E8F6A6FEEB182F6BB` |
| `halo3xr_launcher.exe` SHA-256 | `DA7525BFC4036A6D8F533F92A589C6F495A95CC956F2F7745018CFAC1694870C` |
| Preserved headset evidence | `out/test-runs/03f0bff-reach-alpha-persists-live-20260724-125506Z` |
| Headset result | Stereo, projection, 6DOF, head-owned culling, and stick/head coherence remained good; a translucent fog layer persisted and moved opposite headset motion instead of remaining world-stationary |

The installed hashes matched the package and the first log line reported the
exact source above. A live read of the pinned retail controls proved
`motion_blur_max=0.0` and the finite authored `motion_blur_scale=0.35`, so this
was a valid max-only blur-off result with no zero-over-zero distortion constants.
Reach remained focused with one opaque OpenXR projection layer, two current eye
caches, and zero frame-order failures. The artifact therefore is neither native
motion blur nor a separate OpenXR overlay; it is baked into Reach's rendered
screen-space fog work.

Pinned retail and HREK code then isolated the matching screen-aligned patchy-fog
pass. Retail `player_view_render` tests bit `0x08` at global RVA `0x00CA0240`
at `0x0026CC59`; when clear it calls the patchy helper at
`0x0026CC65 -> 0x0026EFEC`. HREK independently names the corresponding
resources `_surface_patchy_fog_buffer0/1` and `Patchy Fog Global Parameters`.
Source `b0710dc` sets only that proven skip bit immediately around each admitted
VR eye render and restores only that bit in `__finally`, preserving atmospheric
fog, distortion, camera/culling, eye order, capture, and all stock or fallback
renders. Its exact headset result is recorded below.

### Unaccepted Reach patchy-fog headset pass / scale-performance follow-up - 2026-07-24

This result is evidence only and does not advance the accepted pointer above.

| Identity | Value |
| --- | --- |
| Tested runtime source | `b0710dc01b1b6e5deec64830a42d33f19e1a52f1` |
| Candidate package | `out/candidates/b0710dc-reach-camera-20260724-132647552Z` |
| `halo3xr.dll` SHA-256 | `FF43BC89C5AFEC799DA43EB78EC58CC173B113DEC208FEE69E5F2B6235376C35` |
| `halo3xr_launcher.exe` SHA-256 | `AAE13DBDB454DEF58D4922F08C1D7E981E3AE408D84368182540F7D59D043615` |
| Preserved headset evidence | `out/test-runs/b0710dc-reach-patchy-fog-headset-20260724-132935Z` |
| Headset result | The opposite-moving translucent fog layer was gone and the image looked good; remaining follow-up is slightly small world scale versus Halo 3/ODST and a performance dip |

The installed files matched the candidate manifest, and the first log line
reported the exact source above. Reach's cold preflight and display proof passed,
the exact patchy-fog skip was active around each admitted eye, and stereo, head
tracking, and 6DOF armed after the safety interval. Focused frames retained
current private eye caches and zero frame-order failures. Logged stereo samples
were 50, 46, 57, 55, and 58 FPS. The full log SHA-256 is
`4A1B86F38E4799D2A48FF04E70F6316CAC4B7214AC7180BBAE8E6D2BA2F012`.
The same low cadence was already present before Reach loaded or stereo armed;
the per-eye patchy-fog wrapper is therefore not a causal performance regression.

The user explicitly confirmed that this fixed the fog defect. This is a narrow
Reach headset pass, not cumulative acceptance: exact physical-scale calibration,
performance follow-up, and Halo 3 plus ODST regressions remain pending.

### Reach stock runtime observation - 2026-07-23

The external read-only Reach observer from source
`5d34180ca935e7e32d0b1b2beffb014d198c774f` was run against stock
anti-cheat-disabled MCC. This was not a mod installation or candidate launch
and does not change either accepted source pointer.

| Identity | Value |
| --- | --- |
| Observer package | `out/diagnostics/5d34180-reach-runtime-observer-20260723-233050073Z` |
| Observer EXE SHA-256 | `AC43FA4F65256DF1CB46B9C0471DDA97E3120265AEDC876E0A3A73FC6A86CF6A` |
| Preserved run | `out/test-runs/5d34180-stock-reach-observer-20260724-025036448Z` |
| Evidence log SHA-256 | `3C36AF1F06FC428E914AB0C71330838587B020335EFBF2B017F8EF178768212D` |
| Observer result | `OBSERVATIONS_RECORDED_UNASSESSED`, reviewed as limited runtime corroboration |

- Two loaded-image preflights passed: the complete `main_render_view` checks
  were exact, while the frustum check used a unique 24-byte prefix of the
  canonical 25-byte entry. Across two admitted sessions the observer recorded
  29,507 accepted exact-slot transactions, 29,496 valid camera samples,
  seven one-second stable windows, zero invalid cameras, zero multi-owner
  intervals, and zero module-snapshot failures.
- The external observer paused and reset sampling during multi-title
  ambiguity, reran source `5d34180`'s preflight (including its unique 24-byte
  frustum-prefix check) on re-admission, then recorded one Reach unload/title
  exit.
- Every transaction was slot 0, so the run corroborates the array base but not
  the `0xA40` stride or split-screen behavior.
- Subsequent pinned retail/HREK analysis resolved the second caller as Reach's
  screenshot tile/bloom path, established its exact stock-only routing
  requirement, bounded the synchronous camera workspace and its `0x2B0`
  render-scope snapshot, and identified surface group 1 as the swapchain
  display target written by late
  native CHUD. It also selected exact inner candidate `player_view_render`,
  proved its identity and active-scope lifetime, and bounded the stock
  pre-scope camera rebuild. Production outer-owner propagation, live serial
  reuse, inside-scope/OpenXR camera mutation, live target identity/copy, broader
  lifecycle/device-loss behavior, callback quiescence and teardown,
  stereo/OpenXR, and headset behavior remain unproven. All Reach runtime hooks
  remain unauthorized and disabled.

## Accepted cumulative release

The current known-good product is the public
[`MCC_VR_ALPHA_0.2.2`](https://github.com/pancreations/Halo-MCC-VR/releases/tag/MCC_VR_ALPHA_0.2.2)
Halo 3 + ODST release. It supersedes `MCC_VR_ALPHA_0.2.1`.

| Identity | Value |
| --- | --- |
| Release tag commit | `e2c049e5c3b98ce466f6072da4e0aa55ccc88e10` |
| Headset-tested runtime source | `3a2a11bfc66b36e70f60282e91c9d5436f2e18d1` |
| Build | Release x64, `HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP=ON` |
| Release ZIP SHA-256 | `43E52AEF5A2D1647A8F3AE6AEFDB6C22F0C67C7AA06FD70D327FB3E00ACF5DCC` |
| `halo3xr.dll` SHA-256 | `1E3F0F7E1D67DB7F322FF0B2C0236CA8708E4C9EC204EDE83484DBD6BBAF3BD6` |
| `halo3xr_launcher.exe` SHA-256 | `FA95B264630D42594581E4D2F8E1103FE4DB2D0711714DA4F62AA6175155C534` |

The 0.2.2 runtime source `3a2a11b` builds directly on the accepted 0.2.1 line
(`034c4a6`) with three evidence-backed ODST fixes (GitHub issue #18 -
head-relative movement, cinematic FOV parity, and steady rumble) and one XInput
connection-stability fix (the in-game menu no longer dies after Save & Quit).
The tag adds only release documentation and packaging on top of that tested
runtime; `src/` and `tests/` are otherwise the accepted line. The launcher was
rebuilt from the same source, so its bytes differ from 0.2.1 while its behavior
does not.

### Previous accepted release (rollback baseline)

`MCC_VR_ALPHA_0.2.1` remains a protected rollback baseline. Its runtime is
byte-identical to `034c4a6`; the tag added only documentation and packaging.

| Identity | Value |
| --- | --- |
| Release tag commit | `3d7989e1a8e0cb34747a91801c4525ef70b29866` |
| Headset-tested runtime source | `034c4a68e362b334d7994aa9e694243abf2aade5` |
| Release ZIP SHA-256 | `C5AE012BC379CBC7A909652D297DC0E8059CDBF41D26260771B385F8F729B124` |
| `halo3xr.dll` SHA-256 | `B7363F79650E42A04D4CED6A3F51F57A6B4C2F376FF00298A6173A8287752CEF` |
| `halo3xr_launcher.exe` SHA-256 | `BDC0A20F56DF72CDDE68E5D0AB621321FBDE91DA427B6C24142B38336D33EA6D` |

Protected rollback copies of the 0.2.1 ZIP:

- the official GitHub release asset;
- `dist/HaloMCCVR-odst-menu-fix-034c4a6.zip`;
- the user's external safe-folder copy.

An artifact is evidence, not an automatic deployment source. Never install it
unless the user explicitly asks.

### 0.2.2 headset confirmation - 2026-07-23

- Source commit: `3a2a11bfc66b36e70f60282e91c9d5436f2e18d1` (branch
  `cleanup/release-0.2.1`), built Release x64 with
  `HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP=ON`.
- Candidate package: `out/candidates/3a2a11b-20260723-142432262Z`.
- `halo3xr.dll` SHA-256
  `1E3F0F7E1D67DB7F322FF0B2C0236CA8708E4C9EC204EDE83484DBD6BBAF3BD6`; the
  installed file's hash was verified separately after the manual copy and
  matched. The log does not contain the hash.
- Log source: the first line reported `source 3a2a11b...`, ODST ON, compiled
  `Jul 23 2026 09:24:22`, from the canonical `Halo_MCC_VR\halo3xr.log`.
- Title coverage: Halo 3: ODST (stereo, head-relative movement, cutscenes,
  rumble, Save & Quit to menu, and cross-title re-entry). Halo 3 shares the
  touched XInput controller path.
- Result: the user confirmed all three issue #18 fixes still feel right and that
  the in-game menu no longer goes dead after Save & Quit, from both plain
  gameplay and a cutscene.
- Runtime evidence: the retained `M3 DIAG` line held `gateIdle=0` through normal
  play, then rose to `100` and `106` across a Save & Quit / title-teardown
  window while `reads`/`padValid`/`merged` kept climbing. The mod answered the
  slot-0 polls in that brief gated window as connected and idle, so MCC never
  latched a false controller disconnect and the pad stayed live.

## Desktop stale-version audit

This audit predates the 0.2.2 hotfix and describes the earlier 0.2.1 install
(`halo3xr.dll` `B7363F79...`), which was accepted at the time. As of 0.2.2 the
accepted `halo3xr.dll` is `1E3F0F7E...`.

The 2026-07-23 desktop installation is not running an older binary:

- installed DLL hash is the accepted `B7363F79...`;
- installed launcher hash is the accepted `BDC0A20F...`;
- the first log line reports embedded build `Jul 22 2026 12:59:32`;
- the launcher log resolves the canonical `Halo_MCC_VR` folder;
- the desktop shortcut points to that launcher;
- no second mod DLL or launcher exists under the MCC installation.

The old repository build trees contained unaccepted DLLs, and the legacy
scripts could either build ODST support OFF or restore an older vibration DLL.
Those build trees and scripts are not part of the clean baseline. If behavior
still differs from the laptop, investigate configuration, OpenXR/runtime state,
MCC title-module state, and the exact log; do not assume a source rollback.

The audited desktop log also showed the known multi-module ambiguity during
title switching and an ODST camera-readiness tail toggling before the user
paused. Those are runtime observations from the accepted binary, not proof of a
stale install.

### Desktop ODST headset confirmation - 2026-07-23

- Runtime source: `034c4a68e362b334d7994aa9e694243abf2aade5`.
- Installed artifact:
  `N:\SteamLibrary\steamapps\common\Halo The Master Chief Collection\Halo_MCC_VR\halo3xr.dll`,
  SHA-256 `B7363F79650E42A04D4CED6A3F51F57A6B4C2F376FF00298A6173A8287752CEF`.
- Result: the user explicitly confirmed that ODST hooked, worked, and remained
  playable during a brief headset session.
- Runtime evidence: ODST was detected at `06:27:14.962`, its core hooks finished
  installing at `06:27:25.848`, and stereo, head tracking, 6DOF, controller aim,
  VRIK, authored crosshair, and distinct per-eye output armed at `06:27:36.205`.
  The delay covered title/camera loading, an OpenXR visible-but-unfocused
  interval, and the required one-second fresh-camera safety interval.
- A read-only 250-sample layout capture found only slot 0 active; it made no
  game-memory writes. No new build was installed, and no Halo 3 regression was
  run in this brief ODST-only session. The accepted-build pointer is unchanged.

## Headset-confirmed coverage

Halo 3:

- stereo, 6DOF, controller input and aim, head-relative movement;
- articulated arms, support hand, floating hands, and tested dual wield;
- native HUD, authored VR crosshair, HUD controls, scopes, and resolution scale;
- cutscene facing, pause/resume, death/respawn, mission exit/re-entry;
- smooth turn, recoil suppression, haptics, and shared configuration.

ODST on the accepted build:

- stereo, 6DOF, head-relative movement, snap/smooth turn;
- controller-driven gun/hands, arm IK, two-hand and floating-hand options;
- native HUD, authored floating crosshair, HUD controls, and vibration;
- stereo cutscenes with head look and authored-shot facing;
- death/respawn recovery, one tested drivable car, and cross-title re-entry;
- in-game menu stick fix from GitHub issue 9.

## Known limitations

- ODST's first captioned opening cutscene can be black; skip that first scene
  once.
- MCC can retain multiple title modules and return a level load to the menu.
  Fully restart MCC as the release workaround.
- ODST brightness must remain at the game default; the attempted brightness
  hook hid the entire HUD and was reverted.
- Only one ODST car is headset-confirmed. Broader weapon, vehicle, turret,
  passenger-gun, co-op, headset, and long-session coverage remains open.
- Full-body legs/torso are not implemented; VRIK covers first-person arms.
- Projectile direction follows the controller, but Halo still owns the actual
  fire origin.
- A local rebuild is not byte-reproducible because compile date/time and
  toolchain output are embedded. Use the release ZIP for exact accepted bytes.

## Rules that survive cleanup

- One cumulative multi-title line: every accepted build retains Halo 3 and ODST.
- Halo 3 is the player-facing parity foundation for new titles.
- Per-title offsets, signatures, layouts, bones, markers, tags, and calibration
  require per-title evidence.
- Render world, first-person weapon, native CHUD, and capture for each eye as one
  lifecycle transaction after the one-second fresh-camera safety interval.
- Never hook `halo3+0x120DF8`.
- Never write guessed camera, animation, model-root, or CHUD offsets.
- Unique signatures only; fail open to stock rendering.
- Never patch game files or interact with Easy Anti-Cheat.
- A successful candidate package automatically installs only its exact
  manifest-verified DLL/launcher through `tools/install-candidate.ps1`, with MCC
  closed, the prior install preserved, and post-copy hashes verified. It never
  launches MCC or changes `halomccvr.cfg`; restore/uninstall scripts remain
  forbidden.

## Candidate and acceptance workflow

1. Start from this accepted source line.
2. Make one behavioral change and give it a unique commit.
3. Build and test the cumulative Release preset from `BUILDING.md`.
4. Use the safe package command to create a unique candidate under `out/`;
   never overwrite the accepted ZIP or reuse a candidate directory. After every
   successful build/test/package, it automatically backs up the current install,
   deploys that exact candidate, and verifies the installed hashes.
5. Record source commit, DLL hash, unique package path, embedded log
   source/configuration, title coverage, and headset result. Verify the installed
   hash separately because the log does not contain it.
6. Advance this pointer only after explicit acceptance. A failed or untested
   candidate is reverted and does not advance the line.
7. Run a Halo 3 regression whenever shared code or cross-title lifecycle state
   changes.

## Evidence map

- `docs/RE-notes.md`: verified Halo 3 reverse-engineering facts.
- `docs/EDITING-KIT-EVIDENCE.md`: evidence policy.
- `docs/ODST-SIGNATURE-EVIDENCE.md`: ODST signatures and HUD evidence.
- `docs/ODST-CAMERA-LAYOUT.md`: ODST camera/view layouts.
- `docs/ODST-WEAPON-IK-EVIDENCE.md`: ODST weapon and skeleton evidence.
- `docs/REACH-EVIDENCE-MANIFEST.json`: pinned Reach retail/HREK identities and
  preliminary evidence-only RVAs; not an accepted runtime pointer.
- `docs/REACH-SIGNATURE-EVIDENCE.md`: Reach proof ledger; controller transport
  is headset-accepted, while camera-core candidates and their exact headset
  results remain unaccepted until this pointer advances explicitly.
- `docs/TITLE-RUNTIME-OWNERSHIP.md`: accepted shared heartbeat/generation
  ownership contract and its cross-title regression evidence.
- `docs/RESOLUTION-FSR-INVESTIGATION.md`: active (not accepted) findings for the
  resolution-scaling and FSR feature work — verified facts, labeled hypotheses,
  and open questions. No behavioral change shipped.
- `docs/HISTORY.md`: how to retrieve the full pre-cleanup ledger.
- `releases/0.2.2/manifest.json`: current machine-readable release identity.
- `releases/0.2.1/manifest.json`: protected rollback release identity.
