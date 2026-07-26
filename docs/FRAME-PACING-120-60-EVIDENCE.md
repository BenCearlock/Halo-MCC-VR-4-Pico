# Halo MCC VR 120-to-60 frame-pacing evidence

Status: the exact transition diagnostic was captured and assessed on
2026-07-26. A strict worker-order correction is now the selected candidate,
but it is not headset-accepted yet. This document does not advance
`docs/CURRENT-STATE.md`.

## Protected starting point

- User-protected, headset-tested investigation baseline: `a60142a`.
- Its installed DLL SHA-256:
  `3A9CD06DFCE49E63DEB8C40C1C2154FCFE7E7E686B50F94E4A641A78B1290A42`.
- That DLL and its first log line were independently hash/source checked.
- It delivers a real 120 frames/sec and 120 outer stereo view transactions/sec
  while the high cadence is active.
- Preserve the `f410bc6` architecture: `xrWaitFrame` belongs exclusively to a
  dedicated worker, never Halo's render thread.
- Preserve `desktop_present_unlocked`: MCC's shell requests sync interval 1
  while gameplay requests 0; the mirror path must not reintroduce that gate.
- Preserve the fitted/letterboxed desktop mirror. Never stretch it.

The one part of `f410bc6` intentionally changed by the selected candidate is
the handoff order. Releasing the worker only after `xrBeginFrame` left no next
`xrWaitFrame` pending while the runtime entered Begin. The captured trace below
is the evidence for moving that release before Begin; the worker ownership
itself remains intact.

## Do not reopen without contradictory measurement

The following have already been excluded from this flip:

- DWM composition and the desktop-mirror fit;
- SteamVR Motion Smoothing, which was off;
- `resolution_scale`;
- the eye blit, which uses fast `CopyResource` with sample count 1;
- `xrEndFrame`, measured at approximately 0.35-1.66 ms;
- per-frame TRACE logging;
- first-person palette capacity, cache behavior, and zoom/scope logging.

The observed `renderWindow` rises from roughly 5 ms in a light scene to 9-12 ms
while low cadence is active. A measured 8.90 ms frame ran at 119 fps and a
9.14 ms frame ran at 60 fps. Exact edge rows show no render-window threshold or
consistent render-window step at the flip. The aggregate correlation is not
evidence of a render-budget threshold.

## Captured diagnostic identity

- Source: `637c1a73d9ffc98e4fa1b3e9610ed745a999cf03`.
- Package:
  `out/candidates/637c1a7-reach-fp-parity-20260726-132517683Z`.
- Installed DLL SHA-256:
  `BEA82BD91139CCCC40B05ED664DC50B8D07E2A3D8311CC1942EB75F4F268824D`.
- Launcher SHA-256:
  `3955FC3CC6D3B0835AD7DCA058222A075EA1B5CC58393D4EE5A6AD80D3F8C2F5`.
- Captured log SHA-256:
  `2357F893C40CF2648145B0CB718EA9446F0CD8546EA9B9C2C5BBECF81416218D`.
- Queried physical panel rate: 120 Hz.

The run produced 30 exact transition headers and 570 unique clean period-edge
rows after overlapping captures were deduplicated. All were session epoch 1,
worker source `W`, `conflictSeq=0`, coherent, focused, rendering, stereo,
head-tracked, two-layer Halo 3 frames. OpenXR, event, and DXGI results were
clean. The first edge followed gameplay arm; the major recovery and later
sustained drop occurred in normal gameplay with no title, loading, or session
change.

Observed regimes included:

- settling to sustained 60 near serial 996;
- recovery near serial 3529, followed by about 36.7 seconds primarily at 120;
- a 22-frame, roughly 359 ms low-cadence excursion at serials 5176-5197;
- sustained 60 beginning near serial 7636;
- one isolated 30-cadence pulse near serial 9343, then 60.

`predictedDisplayPeriod` is the cadence at which the runtime is asking this app
to submit; it is not necessarily the physical panel period. The UI/log therefore
calls it **app cadence** and reports panel rate separately.

## What the capture ruled out

Every one of the 570 edge rows had the same first-person work signature:

- outer view transactions: `1`;
- palette requests `[eye0/eye1/out]`: `2/2/2`;
- successful full solves: `2/0/2`;
- cache hits: `0/2/0`;
- cache stores: `2/0/0`;
- cache-full events: `0/0/0`.

There was no zoom/scope edge. 569 of 570 rows had zero hot-log counters; the
remaining row contained one ordinary periodic view-rate message. The proposed
four-entry palette-overflow trigger did not occur and is closed for this bug.

The worker handoff was also clean at every edge: worker-owned state, exact
sequence/serial correspondence, coherent epoch, and no overlapping Wait. The
worker's returned Wait packet was already ready (about 0.007 ms observed call
time), yet during the sustained 120-to-60 drop `xrBeginFrame` changed from about
0.024 ms to 1.495 ms, then 7.978 ms, and then roughly 8-12 ms. During recovery,
Begin collapsed from roughly 12 ms through 2.61 ms back to about 0.024 ms.

That is the decisive signature: cadence delay appeared synchronously inside
Begin after Wait(N) had completed, independently of any `renderWindow`
crossover. Exact edge rows do not establish a consistent `renderWindow` step,
so it is not the trigger. Palette work and a 0.24 ms render-time difference
cannot explain the halving.

## Selected correction

The correction implements one strict frame-loop invariant:

1. The worker is the only `xrWaitFrame` caller.
2. It publishes one immutable Wait(N) packet and parks on its exact sequence.
3. The render thread claims and validates that packet, including current session
   epoch and absence of another in-flight Wait.
4. Immediately before Begin(N), it releases the exact N permit.
5. Immediately before calling Wait(N+1), the worker publishes the exact N+1
   dispatch marker. The render thread observes that marker before calling
   `xrBeginFrame` for N; the API call is the worker's next instruction.
6. This positions Wait(N+1) to absorb runtime cadence throttling while Begin(N)
   stays on Halo's render thread. The cross-frame timestamps verify actual call
   ordering in the headset rather than claiming that an application marker can
   observe entry inside the runtime.

This order follows OpenXR's explicit rule that a subsequent `xrWaitFrame` may
be called before the preceding frame is begun, must block until that Begin, and
must unblock independently of End. OpenXR also says runtimes must not perform
frame synchronization or throttling in `xrBeginFrame`:

- <https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrWaitFrame.html>
- <https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrBeginFrame.html>

There is no render-thread/inline Wait fallback. Event handles are wakeup hints;
monotonic sequence predicates are authoritative, so stale event credits and
timeouts cannot authorize another Wait. A missing packet skips XR preparation
and is reported from the non-render worker. Worker startup failure enters a
drain state that retries a session-exit request instead of silently running a
session without a frame loop. An exceptional Begin failure also requests
STOPPING and fail-closes the worker; it never attempts an inline recovery. If
the runtime does not release that outstanding Wait during STOPPING, teardown
deliberately refuses to destroy live handles and reports an unrecoverable
failure. The fatal path runs the existing title-aware render-thread detach
transaction immediately on entry to the drain, and a process-lifetime failure
latch prevents Reach's cold worker from re-arming afterward. The endpoint
repeats the detach defensively. This is error teardown, not an alternate pacing
path.

The worker is created only after a successful `xrBeginSession`, is stopped and
joined before `xrEndSession`, and is rebuilt with a new epoch on session re-entry.
This matches OpenXR's session call-order reset and stop-frame-loop requirements:

- <https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrBeginSession.html>
- <https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrEndSession.html>

The original `predictedDisplayTime` remains paired with its Wait(N)/Begin(N)/
End(N) frame. No period is added and no prediction is retargeted. Scheduling
contains no 60, 72, 80, 90, 120, or 144 Hz branch; all rates remain driven by
the runtime's raw timing.

## Candidate telemetry

The transition recorder remains off render/camera/palette logging paths. Those
paths publish fixed POD counters/records; the existing title worker formats
captures. New `PACING F` fields record the enforced application-side
relationship:

- `dispatchSeq` must equal `waitSeq + 1`;
- `dispatchSeen=1` says the exact next worker dispatch boundary was observed
  before Begin;
- `dispatchAckMs` measures the bounded dispatch handoff;
- `pairExact=1` distinguishes valid cross-frame metrics from their numeric
  sentinel;
- `waitStartVsPrevBeginMs` and `waitEndVsPrevBeginMs` correlate the Wait(N+1)
  wrapper against Begin(N). A negative start and nonnegative end show that the
  wrapper interval straddled Begin; the headset result and Begin/worker
  durations determine whether runtime throttling actually transferred.

`exact=0`, a nonzero conflict, `dispatchSeen=0`, an epoch/sequence
discontinuity, packet miss, signal failure, or frame-order failure invalidates
causal reading of that capture. Do not interpolate across it.

## Headset acceptance required

Do not call this fixed until the exact newly packaged DLL hash passes:

- Halo 3 high/low scene reproduction long enough to cover former 120/60 flips;
- `dispatchSeen=1`, exact N/N+1 sequences, low `dispatchAckMs`, and no
  packet/order failures;
- `xrBeginFrame` remaining short while runtime delay moves to the worker Wait;
- level exit and re-entry, headset focus loss/re-entry, and clean MCC close;
- Halo 3 regression plus another supported-title lifecycle regression because
  frame-loop session handling is shared.

Packaging and installation do not advance the accepted pointer. Only the
headset result for that independently verified installed hash can do so.
