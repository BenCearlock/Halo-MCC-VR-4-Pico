# Current state

Authoritative as of 2026-07-23. This file is the only active accepted-build
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

- Two exact loaded-image preflights passed. Across two admitted sessions the
  observer recorded 29,507 accepted exact-slot transactions, 29,496 valid camera samples,
  seven one-second stable windows, zero invalid cameras, zero multi-owner
  intervals, and zero module-snapshot failures.
- The external observer paused and reset sampling during multi-title
  ambiguity, ran a full preflight on re-admission, then recorded one Reach
  unload/title exit.
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
- No automatic deploy, restore, install, uninstall, or launch scripts.

## Candidate and acceptance workflow

1. Start from this accepted source line.
2. Make one behavioral change and give it a unique commit.
3. Build and test the cumulative Release preset from `BUILDING.md`.
4. Use the safe package command to create a unique candidate under `out/`;
   never overwrite the accepted ZIP or reuse a candidate directory.
5. Deploy only after the user explicitly requests that exact candidate.
6. Record source commit, DLL hash, unique package path, embedded log
   source/configuration, title coverage, and headset result. Verify the installed
   hash separately because the log does not contain it.
7. Advance this pointer only after explicit acceptance. A failed or untested
   candidate is reverted and does not advance the line.
8. Run a Halo 3 regression whenever shared code or cross-title lifecycle state
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
  is headset-accepted while all Reach runtime hooks remain unauthorized and
  disabled.
- `docs/TITLE-RUNTIME-OWNERSHIP.md`: accepted shared heartbeat/generation
  ownership contract and its cross-title regression evidence.
- `docs/HISTORY.md`: how to retrieve the full pre-cleanup ledger.
- `releases/0.2.2/manifest.json`: current machine-readable release identity.
- `releases/0.2.1/manifest.json`: protected rollback release identity.
