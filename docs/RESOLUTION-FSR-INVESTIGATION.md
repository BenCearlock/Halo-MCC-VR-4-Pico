# Resolution + FSR investigation (active, not an accepted change)

Status: **Stage 1 desktop-fit correction in progress; no resolution/FSR change is
accepted.** This records verified findings, failed candidates, labeled
hypotheses, and the evidence gates for resolution scaling and FSR. It does **not**
advance the accepted pointer in `docs/CURRENT-STATE.md`. Updated 2026-07-25.

## Session context (where the branch is)

- Branch `reach/campaign-parity`; the authoritative accepted pointer remains the
  commit named by `docs/CURRENT-STATE.md`. Current Reach and resolution work is
  cumulative but unaccepted.
- Failed predecessor `a440654`: package
  `out/candidates/a440654-reach-fp-parity-20260725-232800404Z`, DLL SHA-256
  `E4CEF28463717763F012DA0E8C407E7DC60151EE1FB3657A8BD820369AED9684`.
- Failed cursor-containment successor `c6385db`: package
  `out/candidates/c6385db-reach-fp-parity-20260726-011531523Z`, DLL SHA-256
  `4394C169D4AD28EDBE41498B20494EC6C3DB144B4F48976476A9676890794691`.
  Its exact failed run is archived under
  `out/test-runs/c6385db-desktop-fit-menu-fail-20260726-011903Z`.
- This track covers desktop/render decoupling first, then the 8K-class cap and
  tiers, then FSR as a separate diagnostic-first behavior.

## What the user wants

1. Change resolution without a full game restart — ideally re-applied on the
   Start (pause) menu transition, reading the value set in the F1 menu.
2. Raise the cap to 8k-class and rescale the named tiers so **Keith David = 8k**;
   add a warning or soft lock past ~5k so users don't crash weak systems.
3. An F1 control for **FSR** (MCC's built-in FidelityFX upscaler). The user
   enabled MCC's FSR and got a "weird second screen" and a wrong FOV/projection.
   FSR must work across **all MCC titles** (persistent feature), not only the
   three the mod ships today. The user suspects the resolution revert **might be
   ODST-specific** (untested — their captured logs were Halo 3).

## Stage 1 desktop-fit evidence (verified, still unaccepted)

- `546d301` preserved the full `3204x2310` backbuffer while fitting the visible
  window inside a `1280x720` monitor. The user confirmed that the full picture
  was visible on the monitor and the headset render remained complete. It did
  not make every MCC shell input path operable, so it is not accepted.
- `a440654` tried to repair the shell by rewriting every non-mod
  `user32!GetCursorPos` call. Its first three logs all ran before the window
  shrink while client and render were both `3204x2310`; they provide no
  post-fit transform evidence. The user then confirmed mouse, keyboard, and
  controller could all fail in the fitted shell. Reach gameplay could run only
  in a session that happened to get past the shell.
- Controller transport was not lost. In the stalled `a440654` run, XInput
  reached `reads=159619`, `merged=152798`, with D-pad input observed, but no
  title loaded.
- A historical live observation from a successful `a440654` session began with
  physical cursor `(1126,555)`, inside the fitted window; its complete log is no
  longer retained. The durably archived stalled session began at `(1244,624)`,
  outside the fitted outer rectangle (approximately `x=141..1138`). The old fit
  never relocated the cursor and its transform explicitly skipped outside
  points.
- Static analysis of the pinned MCC executable (SHA-256
  `BE70D6DCD1A884F10CEB342A7A2DCB35EE0FA43181B66A1D19C3D830E9834691`)
  found three direct `GetCursorPos` consumers:

  - `MCC+0x87D785` is a conditional selected-input-record path. It immediately
    feeds `ScreenToClient` and stores two floats, but is not proven to be the
    top-level shell cursor poll.
  - `MCC+0xEE911E` is vtable slot 0 of MCC's Windows cursor object. It performs
    `GetCursorPos -> GetActiveWindow -> ScreenToClient -> DPI scale ->
    ClientToScreen` and returns two floats, making it the stronger static Slate
    cursor candidate.
  - `MCC+0xEEBD40` immediately feeds `WindowFromPoint` to decide whether the
    physical point belongs to an MCC window. This call must remain physical.

- `8fa36d6` removed the broad rewrite and transformed only the first path above.
  Exact packaged/deployed identity:

  | Identity | Value |
  | --- | --- |
  | Source | `8fa36d6c3b667043b3ea1171a68ac75aed3286ef` |
  | Package | `out/candidates/8fa36d6-reach-fp-parity-20260726-002302958Z` |
  | `halo3xr.dll` SHA-256 | `52342D585F1190F53E6004ED2DA581DA9FC266CE290197070203818A7534EAB2` |
  | Headset result | User: "still can't navigate menus and pause screens properly" |

  Its log resolved the signature but recorded zero post-fit calls to
  `MCC+0x87D785`. The run nevertheless delivered 22,231 XInput reads and
  16,024 merged VR-controller states; A selected Reach from the shell,
  Menu/Start reached Reach, and Alt+F4 reached MCC's WndProc before clean title
  teardown. Therefore the candidate did not lose controller/keyboard transport,
  but it also did not correct the reported navigation path. The exact failed
  evidence is under
  `out/test-runs/8fa36d6-desktop-fit-input-fail-20260725-192527Z`.
- The same exact run exposed a separate Reach defect. Reach remains
  `RuntimeMode::Loading` while its camera is armed, and
  `Game_MoveStickIsLocomotion` explicitly treats that armed state as gameplay
  even after the native pause menu opens. Ordinary pause-menu stick input is
  consequently head-rotated/deadzone-floored locomotion rather than the shared
  plain menu-stick path. No authoritative Reach pause-state proof exists yet;
  this must be corrected as a separate title-runtime candidate, not inferred
  from Start edges or stacked into desktop fit.
- `c6385db` tested one-time cursor containment during the fitted-window shrink.
  The exact log proves the guarded path fired and moved the verified game-owned
  cursor from `(916,20)` to `(429,36)` while the client changed from
  `3204x2310` to `982x681`. Menu navigation still failed. XInput remained live,
  four A edges reached MCC, Reach loaded, and the F1 overlay toggled twice. The
  containment theory is falsified and the behavior was removed in `0819a0d`.
- `0819a0d` restores the display-fit runtime files exactly to `546d301`: no
  broad `GetCursorPos` rewrite, selected-caller rewrite, or cursor relocation is
  retained. The fitted window and forced full-size headset backbuffer remain.
- `222d08f` removed only `546d301`'s fitted-window `WM_MOUSE*` lParam
  remapping. The exact installed source appeared in the first log line and the
  fitted client remained active, but the user reported that native menu
  navigation was unchanged. That isolates the stock-vs-scaled message
  coordinate theory as another no-effect path; the user explicitly requested
  that the current fitted-window work not be reverted.
- `4424c10` made the fitted MCC window borderless before resizing it into the
  monitor work area, on the reproduced community claim that borderless preserves
  native shell/pause navigation. The user reported the fit still worked
  (display), but the menu was unchanged.
- **User headset evidence 2026-07-25 (the decisive read).** On the fitted
  window the pointer responds only in the **top-left** region, and with the
  keyboard alone the highlight **moves but stops short** of the lower items
  (Halo 3 / ODST / Quit). Both symptoms are one root cause: MCC lays out and
  hit-tests its native shell/pause menu in the **full render space**
  (e.g. 3204x2310), but the OS cursor that drives every selection -- the mouse
  AND the gamepad/keyboard "virtual cursor" the console-style shell moves -- is
  confined to the **small physical window**, so only the top-left window-sized
  slice is reachable. This is a coordinate-domain mismatch, not a focus or
  borderless issue, and it is what every prior candidate mis-targeted.
- **Cursor coordinate remap candidate (this change, `src/dll/d3d11_hook.cpp`).**
  Make MCC's OS-cursor space match the fitted window in both directions, scoped
  to callers inside the game executable image (never the mod's own ImGui overlay
  or a system DLL), no hardcoded game address:
    - `GetCursorPos` -> scale the physical, window-confined point UP into full
      render space (uniform; the fitted window preserves render aspect).
    - `SetCursorPos` -> scale MCC's render-space cursor target back DOWN into the
      window so gamepad/keyboard nav stays inside it and the round-trip is exact.
    - `WindowFromPoint` -> undo the remap for exactly the value last handed out,
      so MCC's post-`GetCursorPos` "is the cursor still over my window?" check
      keeps seeing the TRUE physical point. Feeding it the scaled-up point (which
      lands outside the small window) is why the earlier broad `a440654` rewrite
      dropped all input; this value-scoped undo avoids identifying that caller by
      address. Bounded log lines `fit: menu cursor read ...` /
      `fit: menu cursor move ...` record the first calls (caller RVA + before/
      after coords) so, if a path is still not covered, MCC's cursor model is
      proven rather than guessed. Built, tests pass; headset-PENDING.

Evidence logs:

- `out/deploy-backups/e4cef28-before-8fa36d6-20260726-002303744Z/halo3xr.log`
  (stalled `a440654` shell session)
- `out/test-runs/8fa36d6-desktop-fit-input-fail-20260725-192527Z/halo3xr.log`
- `out/test-runs/c6385db-desktop-fit-menu-fail-20260726-011903Z/halo3xr.log`
  (failed `c6385db` cursor-containment session)

## How resolution works today (verified)

- `src/launcher/launcher.cpp` reads `resolution_scale` (0.35–2.00) and passes
  `-WINDOWED -ResX -ResY` = `kNativeRenderWidth/Height` (2912×2100, `config.h`)
  × scale **at launch only**. Nothing changes it later.
- The DLL captures Halo's scene render target and upscales the eye into the fixed
  full-size OpenXR projection. `EnsureEyeCaches` in `src/dll/vr.cpp` rebuilds
  the capture at whatever size Halo is rendering, logging
  `M2: persistent eye frame caches created: WxH`. This is the "it rescales every
  time it rehooks" the user observed — the mod already adapts to a size change; it
  just never *drives* one.
- Hard rule (`docs/RE-notes.md` "Resolution and upscaling"): scale Halo's **source
  raster** uniformly; never shrink the OpenXR swapchain / submitted imageRect.
- Tier constants are mirrored in three files that must stay in sync:
  `config.cpp`, `launcher.cpp`, `menu.cpp`. Current tiers: Potato .50, Low .67,
  Medium .80, High 1.00, Ultra 1.10, Keith David 1.50. `kResolutionScaleMax=2.00`.

## Verified findings (evidence, not guesses)

- **No engine resolution variable was found in the prior session.** Its recorded
  scan covered all three game DLLs' debug-var tables (the same mechanism that found
  `render_far_clip_distance`). Only `allow_480p_resolutions`,
  `render_debug_depth_render_scale_*`, `render_screen_res` (Reach, value 0), and
  `texture_camera_set_resolution` (an hs_function) exist. **None** is a usable
  scene render-scale. So resolution **cannot** be driven by name the way
  `draw_distance` / `motion_blur` are. The lever is MCC-level (the DXGI
  swapchain), not the Halo engine. The referenced one-off scan script and raw
  output were not retained in this repository, so repeat that offline scan before
  treating the negative result as a production proof.
- **A mod-initiated swapchain resize is not possible.** DXGI forbids
  `ResizeBuffers` while another module (MCC) holds references to the swapchain's
  back buffers; the call fails. MCC owns its swapchain. The mod already **hooks**
  `ResizeBuffers` through `ResizeBuffersHook` in `src/dll/d3d11_hook.cpp` and
  can rewrite the width/height MCC passes — but only when **MCC itself**
  initiates a resize.
- **The revert-on-Start is setup-specific and did NOT reproduce for the user.**
  User's log (4K panel 3840×2160): launched scale 1.50 → swapchain **4368×3150**,
  which is larger than the panel in both dimensions, and it was **honored and
  stable** across a Start pause/resume — **no** `resized its swapchain` line at
  all. So "over 1.0 goes back" does not happen on this machine; other users' MCC
  clamps the windowed backbuffer to their panel. A revert-fix candidate is
  therefore **not user-testable** on the current machine.
- **Halo 3 and ODST Start transitions are already detected** by the mod
  (`Runtime mode: gameplay → paused` / `paused → gameplay`), so those adapters
  have a clean runtime trigger. Reach is the exception: it does not yet publish
  authoritative pause state.
- **OpenXR eye targets are runtime-recommended and fixed per session, not
  globally ~3400×3468.** The exact `8fa36d6` run recommended `4164×4244` per
  eye. The source-to-eye supersampling threshold therefore depends on the
  runtime/headset; 8k-class source resolution can still bring real
  anti-aliasing gains with steep GPU cost and diminishing returns. This should
  shape the safety warning framing.
- **FSR is invisible to the mod.** Enabling MCC's FSR produced **zero** recognized
  log events. The mod captures and reprojects the full frame assuming FSR isn't
  touching the render target/viewport, which is the likely cause of the
  second-screen / wrong-FOV symptom.
- **There is no FSR implementation in this repository.** No config key, F1
  control, launcher argument, FidelityFX dependency/shader, or MCC-setting owner
  exists. The current final eye expansion uses an ordinary linear sampler.
- **MCC FSR, OpenXR Toolkit FSR, and a future mod-owned eye upscaler are
  different transactions.** The old Toolkit tiled/overlap report must not be
  treated as proof of what MCC's built-in setting does.
- **The current capture has specific FSR blind spots.** Once it learns one exact
  full-backbuffer scene RTV, it ignores different RTVs until resize/title
  detach. It observes render-target binds but not viewport, scissor, or
  compute/UAV output changes. A stale pre/post-upscale target or a changed
  active sub-rect can therefore produce the reported duplicate/wrong-scale
  image without a swapchain-resize log. This mechanism is code-backed but
  runtime-unproven.

## Hypotheses (explicitly unproven)

- **Panel-clamp (affected users):** native height 2100 fits under a 4K 2160 panel
  at scale ≤1.0; above ~1.03 the height exceeds the panel and MCC clamps the
  windowed backbuffer when it touches the window on Start. Matches the reported
  "over 1.0" boundary but is not confirmed on hardware that reproduces it.
- **ODST-specificity:** the user suspects the revert may be ODST-only. Untested —
  captured logs so far are Halo 3. Would need an ODST repro log
  (`game resized its swapchain` / `persistent eye frame caches created` lines).

## Open questions / suggested evidence before coding

- **FSR (diagnostic first):** what does MCC's FSR do to the scene render target
  and viewport? A read-only probe logging the scene-color desc (size/format) and
  bound viewport when FSR is toggled on/off would reveal whether FSR shrinks the
  render target (so the mod must capture the pre-upscale target) or changes the
  viewport rect (so the mod's projection assumptions break). The mod already sees
  the depth-stencil and RTV in `OMSetRenderTargets` → `VR_RedirectRenderTargets`
  and logs swapchain resizes, so most of the instrumentation hooks already exist.
  The probe must use fixed storage in the D3D hot hooks and emit the completed
  bounded snapshot later at Present. Compare the exact same DLL hash with MCC
  FSR Off and On, desktop fit disabled, Halo 3 first and then ODST. Capture
  slot-0 RTV/UAV identity, dimensions/format/bind flags, viewport/scissor,
  eye-cache, backbuffer, XR destination, and final blit path. Do not add an F1
  toggle or alter capture behavior until that transaction is proven.
- **Cap + tiers + safety (safe, user-testable):** raising `kResolutionScaleMax`
  and rescaling the tiers is constants-only across `config.h`, `config.cpp`,
  `launcher.cpp`, `menu.cpp`, plus an F1 warning past ~5k. The user can test how
  high their own rig actually renders (finds the true ceiling MCC will honor). 8k
  ≈ uniform scale ~2.64 (≈7680 wide); 5k ≈ ~1.76.
- **Live resolution:** universal live internal-resolution change is a dead end
  without MCC reverse-engineering (MCC owns the swapchain). The only live lever is
  rewriting MCC's **own** `ResizeBuffers` when it fires — which helps only users
  whose MCC resizes on Start, and needs a hardware repro to validate.

## File / reference map

- `src/launcher/launcher.cpp` — reads `resolution_scale`, emits `-ResX/-ResY`.
- `src/common/config.h` — `kNativeRenderWidth/Height`,
  `kResolutionScaleMin/Max`, `resolution_scale`.
- `src/common/config.cpp` — clamp, parse, save.
- `src/dll/menu.cpp` — F1 "Resolution scale" slider + named tier presets.
- `src/dll/d3d11_hook.cpp` — `PresentHook`, `Present1Hook`, and
  `ResizeBuffersHook`.
- `src/dll/vr.cpp` — `EnsureEyeCaches` (adaptive capture size),
  `VR_OnResizeBuffers` / `VR_AfterResizeBuffers`, and
  `VR_RedirectRenderTargets` (scene-color RTV learn).
- `docs/RE-notes.md` "Resolution and upscaling" — the non-negotiable rules.
