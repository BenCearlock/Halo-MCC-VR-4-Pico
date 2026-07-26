# Halo MCC VR 120-to-60 frame-pacing evidence

Status: transition diagnostic prepared on 2026-07-26; headset acceptance is
still required. This document does not advance `docs/CURRENT-STATE.md`.

## Protected starting point

- Source: `a60142a`.
- Installed DLL SHA-256:
  `3A9CD06DFCE49E63DEB8C40C1C2154FCFE7E7E686B50F94E4A641A78B1290A42`.
- The installed DLL and first log line were independently hash/source checked.
- The build delivers a real 120 frames/sec and 120 outer stereo view
  transactions/sec when the high cadence is active.
- Preserve `f410bc6`: `xrWaitFrame` runs on the dedicated wait thread and that
  thread is released only after successful `xrBeginFrame`.
- Preserve `desktop_present_unlocked`: MCC's shell requests sync interval 1
  while gameplay requests 0; the mirror path must not reintroduce that gate.

## Do not reopen without contradictory measurement

The following have already been excluded from this flip:

- DWM composition and the fitted/letterboxed desktop mirror. Never stretch the
  desktop mirror; that presentation was explicitly rejected.
- SteamVR Motion Smoothing, which was off.
- `resolution_scale`.
- The eye blit, which uses the fast `CopyResource` path with sample count 1.
- `xrEndFrame`, measured at approximately 0.35-1.66 ms.
- Per-frame TRACE logging.

The observed `renderWindow` rises from roughly 5 ms in a light scene to 9-12 ms
while the low cadence is active. A measured 8.90 ms frame ran at 119 fps and a
9.14 ms frame ran at 60 fps. That 0.24 ms difference does not establish a
missed-frame budget threshold. Treat the inflated aggregate as a possible
consequence of the runtime-selected low cadence until transition-correlated
evidence says otherwise.

## Corrections to earlier interpretations

`XrFrameState::predictedDisplayPeriod` is the period at which the runtime is
currently asking the application to produce frames. It is not necessarily the
physical panel refresh. The menu/log now names it **app cadence** and reports
the separately queried panel rate alongside it.

After `f410bc6`, the legacy `xrWait p95` timer at `PrepareNextFrame` measures the
render thread's event handoff. It does not measure the worker's actual
`xrWaitFrame` call. Its log label is now `wait handoff p95`; the transition
recorder carries the worker call's own QPC start/end timestamps.

The statement "268 palette solves/sec is about 4.5 solves per frame" was not
supported by the adjacent rate. The nearby outer-view rate was about 67/sec, so
268/67 is 4.0 successful full solves per frame. The separate 654 solves/sec at
about 120 outer views/sec is a real anomaly: approximately 5.45 solves/frame
over that logging interval.

## Palette-cache coding suspect

`FpStereoSolveScope::palettes` has four entries. Cache lookup reuses an exact
source palette; cache insertion silently stops after four unique palettes. If a
stereo transaction submits five unique visible palettes per eye, the expected
counter signature is:

- First-rendered eye: 5 successful full solves, 4 stores, 1 cache-full event.
- Second-rendered eye: 4 hits, 1 repeated full solve, 1 cache-full event.

That produces six successful full solves for the transaction. The physical eye
number that renders first depends on `right_eye_first`.

The observed zoom interval is compatible with this capacity-overflow pattern,
but it is not yet proof and it cannot explain every transition: later cadence
flips were observed without zoom activity. `fullSolves` specifically means a
successful, non-explicit arm-IK cache miss; it excludes rigid fallback and
failed reconstruction. The `out` bucket means `g_stereoEye == -1` and includes
scope plus any other outside-eye reconstruction.

## Exact transition recorder

The diagnostic descendant of `a60142a` adds no formatting, allocation, lock, or
file I/O to render/camera/palette hooks. Those paths update relaxed monotonic
counters and publish one immutable record after Present into a fixed 1024-slot
single-producer/single-consumer queue. The existing 50 ms title worker drains
the queue and performs all capture formatting.

A transition is detected from consecutive raw periods when their relative
difference exceeds five percent. There are no hard-coded 60/72/90/120/144 Hz
values. Captures contain up to 64 preceding frames, the trigger frame, and 128
following frames. Every row is keyed by prepared-frame serial and includes:

- raw predicted time/period and derived app cadence;
- actual wait-worker call, ready handoff, render event wait, `xrBeginFrame`,
  preparation, game render, submit construction, `xrEndFrame`, and exact DXGI
  Present timestamps;
- wait sequence, session epoch, inline/overlap/coherency flags, and results;
- H3 outer view transactions;
- per-eye/outside-eye palette requests, successful full solves, cache hits,
  stores, and cache-full events;
- synchronous hot-hook log writes for zoom, view-rate, palette-rate,
  camera-rate, and FP-driver-rate messages.

The worker prints `PACING TRANSITION CAPTURE`, followed by `PACING F` timing rows
and matching `PACING C` counter rows. `exact=0` means a queue gap, wait-packet
overwrite/epoch mismatch, inline wait, overlapping wait call, or wait-sequence
discontinuity occurred; do not silently interpolate across it.

## Wait-thread hazards kept separate from this diagnostic

The accepted normal wait-thread path remains intact. Static review found broken
fallback/lifecycle edges that the recorder exposes but this candidate does not
behaviorally change:

- A render-thread timeout may invoke inline `xrWaitFrame` while the worker still
  has an `xrWaitFrame` outstanding.
- The worker's consumed-event timeout may begin another wait without a matching
  `xrBeginFrame` for the prior state.
- The worker is not quiesced and reset across all session lifecycle boundaries.
- Failure/valid event state can become stale or race on abnormal paths.

OpenXR permits `xrWaitFrame` on a thread different from `xrBeginFrame`, so the
proven decoupling itself is not the defect. Concurrent `xrWaitFrame` calls are
not a legal recovery mechanism. Harden these edges only as a separate candidate
after the transition capture identifies whether one occurs at the cadence flip.

## Headset test needed

Run the exact packaged hash until at least one 120-to-60 or 60-to-120 app-period
transition occurs, then close MCC and preserve `halo3xr.log`. Do not infer the
cause from a ten-second p95 line; compare the `PACING F` and `PACING C` rows at
`rel=-1`, `rel=0`, and the immediate post-transition frames.
