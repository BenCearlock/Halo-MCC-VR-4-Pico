# Resolution + FSR investigation (active, not an accepted change)

Status: **Stage 1 desktop-fit correction in progress; no resolution/FSR change is
accepted.** This records verified findings, failed candidates, labeled
hypotheses, and the evidence gates for resolution scaling and FSR. It does **not**
advance the accepted pointer in `docs/CURRENT-STATE.md`. Updated 2026-07-25.

## Session context (where the branch is)

- Branch `reach/campaign-parity`; the authoritative accepted pointer remains the
  commit named by `docs/CURRENT-STATE.md`. Current Reach and resolution work is
  cumulative but unaccepted.
- Installed failed input candidate before this correction: source `a440654`, DLL
  SHA-256 `E4CEF28463717763F012DA0E8C407E7DC60151EE1FB3657A8BD820369AED9684`;
  package `out/candidates/a440654-reach-fp-parity-20260725-232800404Z`.
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
  window inside a `1280x720` monitor. The user confirmed that the full picture was
  visible on the monitor and the headset render remained complete. It did not
  make MCC's shell operable, so it is not accepted.
- `a440654` tried to repair the shell by rewriting every non-mod
  `user32!GetCursorPos` call. Its first three logs all ran before the window
  shrink while client and render were both `3204x2310`; they provide no post-fit
  transform evidence. The user then confirmed mouse, keyboard, and controller
  could all fail in the fitted shell. Reach gameplay could run only in a session
  that happened to get past the shell.
- Controller transport was not lost. In the stalled run, XInput reached
  `reads=159619`, `merged=152798`, with D-pad input observed, but no title loaded.
- The successful a440 session began with physical cursor `(1126,555)`, inside the
  fitted window. The stalled session began at `(1244,624)`, outside the fitted
  outer rectangle (approximately `x=141..1138`). The old fit never relocated the
  cursor and its transform explicitly skipped outside points.
- Static analysis of the pinned MCC executable (SHA-256
  `BE70D6DCD1A884F10CEB342A7A2DCB35EE0FA43181B66A1D19C3D830E9834691`) found
  exactly three direct `GetCursorPos` consumers. `MCC+0x87D785` immediately feeds
  `ScreenToClient` and stores two float coordinates; `MCC+0xEEBD40` immediately feeds
  `WindowFromPoint`; `MCC+0xEE911E` performs a separate active-window/DPI
  conversion. Feeding a synthetic render-space point to `WindowFromPoint` can
  make MCC reject its own window, a concrete mechanism consistent with the
  all-input failure.
- The current correction therefore removes the broad rewrite, resolves only the
  observed GetCursorPos-to-ScreenToClient coordinate caller through a unique
  relocation-tolerant AOB plus imported-call identity checks, and keeps all
  physical window/focus consumers stock. Bounded post-fit logs distinguish a
  scaled in-client point from an outside point. If the exact input signature or
  hook is unavailable, the swapchain and window geometry both remain stock. This
  remains headset-pending until its exact packaged hash passes the three-title
  matrix.

Evidence logs:

- `out/deploy-backups/1ba1103-before-a440654-20260725-232801242Z/halo3xr.log`
- installed `Halo_MCC_VR/halo3xr.log.prev` (successful shell session)
- installed `Halo_MCC_VR/halo3xr.log` (stalled shell session)

## How resolution works today (verified)

- `src/launcher/launcher.cpp` reads `resolution_scale` (0.35–2.00) and passes
  `-WINDOWED -ResX -ResY` = `kNativeRenderWidth/Height` (2912×2100, `config.h`)
  × scale **at launch only** (launcher.cpp:217-232). Nothing changes it later.
- The DLL captures Halo's scene render target and upscales the eye into the fixed
  full-size OpenXR projection. `EnsureEyeCaches` (`src/dll/vr.cpp:1176`) rebuilds
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
  `ResizeBuffers` (`src/dll/d3d11_hook.cpp:108`) and can rewrite the width/height
  MCC passes — but only when **MCC itself** initiates a resize.
- **The revert-on-Start is setup-specific and did NOT reproduce for the user.**
  User's log (4K panel 3840×2160): launched scale 1.50 → swapchain **4368×3150**,
  which is larger than the panel in both dimensions, and it was **honored and
  stable** across a Start pause/resume — **no** `resized its swapchain` line at
  all. So "over 1.0 goes back" does not happen on this machine; other users' MCC
  clamps the windowed backbuffer to their panel. A revert-fix candidate is
  therefore **not user-testable** on the current machine.
- **The Start transition is already detected** by the mod
  (`Runtime mode: gameplay → paused` / `paused → gameplay`), so there is a clean
  runtime trigger available.
- **Per-eye OpenXR target is fixed at ~3400×3468.** Source scale past ~1.17
  (3400/2912) is **supersampling** — real anti-aliasing gains but steep GPU cost
  and diminishing returns. 8k is mostly a "burn GPU for smoother edges" option,
  not raw detail. This should shape the safety warning framing.
- **FSR is invisible to the mod.** Enabling MCC's FSR produced **zero** recognized
  log events. The mod captures and reprojects the full frame assuming FSR isn't
  touching the render target/viewport, which is the likely cause of the
  second-screen / wrong-FOV symptom.
- **There is no FSR implementation in this repository.** No config key, F1
  control, launcher argument, FidelityFX dependency/shader, or MCC-setting owner
  exists. The current final eye expansion uses an ordinary linear sampler.
- **MCC FSR, OpenXR Toolkit FSR, and a future mod-owned eye upscaler are different
  transactions.** The old Toolkit tiled/overlap report must not be treated as
  proof of what MCC's built-in setting does.
- **The current capture has specific FSR blind spots.** Once it learns one exact
  full-backbuffer scene RTV, it ignores different RTVs until resize/title detach.
  It observes render-target binds but not viewport, scissor, or compute/UAV
  output changes. A stale pre/post-upscale target or a changed active sub-rect can
  therefore produce the reported duplicate/wrong-scale image without a
  swapchain-resize log. This mechanism is code-backed but runtime-unproven.

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
  bounded snapshot later at Present. Compare the exact same DLL hash with MCC FSR
  Off and On, desktop fit disabled, Halo 3 first and then ODST. Capture slot-0
  RTV/UAV identity, dimensions/format/bind flags, viewport/scissor, eye-cache,
  backbuffer, XR destination, and final blit path. Do not add an F1 toggle or alter
  capture behavior until that transaction is proven.
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

- `src/launcher/launcher.cpp:94-232` — reads `resolution_scale`, emits
  `-ResX/-ResY`.
- `src/common/config.h:9-16,131-135` — `kNativeRenderWidth/Height`,
  `kResolutionScaleMin/Max`, `resolution_scale`.
- `src/common/config.cpp:59,219-220,494-495` — clamp, parse, save.
- `src/dll/menu.cpp:431-457` — F1 "Resolution scale" slider + named tier presets.
- `src/dll/d3d11_hook.cpp:81-117` — Present / Present1 / **ResizeBuffers** hooks.
- `src/dll/vr.cpp:1176-1211` — `EnsureEyeCaches` (adaptive capture size).
- `src/dll/vr.cpp:4692-4735` — `VR_OnResizeBuffers` / `VR_AfterResizeBuffers`.
- `src/dll/vr.cpp:5110+` — `VR_RedirectRenderTargets` (scene-color RTV learn).
- `docs/RE-notes.md` "Resolution and upscaling" — the non-negotiable rules.
