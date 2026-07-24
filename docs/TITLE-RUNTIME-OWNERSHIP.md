# Shared title-runtime ownership candidate

Status: **desk-tested, headset-untested, unaccepted**.

The accepted baseline remains public release `MCC_VR_ALPHA_0.2.2`, runtime
source `3a2a11bfc66b36e70f60282e91c9d5436f2e18d1`. This candidate does not
advance `docs/CURRENT-STATE.md` and must not be installed or launched without
explicit approval for its exact packaged DLL hash.

## Exact parity behavior being preserved

Halo 3 and ODST keep the accepted 0.2.2 camera ownership, controls, stereo,
native CHUD, first-person weapon, capture, pause, fallback, and teardown paths.
This candidate does not extract, reorder, or add work inside either title's
two-eye render transaction.

Its one intended behavior change is title selection when MCC keeps more than
one game DLL resident. Module presence now means only **available**. A title is
the runtime owner only when its currently installed, non-teardown lifecycle
generation publishes the one unique fresh camera heartbeat. Zero or multiple
qualifying titles expose no owner and no capabilities.

## Rejected first package and retained fix

The exact `38c480a` package (`halo3xr.dll` SHA-256
`C6B49BD0F94E2F2366FDEBDC71D8359123A4FBA2035B381A5BF3478B9952B290`)
was rejected during its first Halo 3 headset regression. Gameplay controller
input, stereo, 6DOF, native HUD/weapon presentation, pause, resume, and title
exit all ran, but ordinary VR-pad input stopped after Save & Quit while MCC
held a multi-resident `Unknown` module set in the frontend.

The failure did not invalidate generation-tagged ownership. The resolver
correctly returned zero owner and zero capabilities after the module-set epoch
changed; the controller admission call site incorrectly treated that as a
reason to suppress title-independent frontend input. The retained fix restores
the accepted 0.2.2 rule only for ordinary controller transport in `None` or
multi-resident `Unknown` frontend/transition states. A unique owner may also
publish `ControllerInput`. Explicit Reach, CE, H2, and H4 states remain stock,
and stereo, aim, HUD, IK, room scale, runtime modes, and haptics receive no
fallback capability. The fabricated XInput slot also remains connected across
gated haptic transitions; stale haptic amplitude is still cleared.

The installed test files were restored from the verified official 0.2.2 ZIP.
The source foundation was retained and corrected for a new uniquely hashed
replacement candidate.

## State and transition rules

- The fixed title table tracks the exact loaded-module mask and base address.
  A title's generation changes when that title loads, unloads, reloads, or is
  rebound at another base. A separate epoch changes whenever any mask/base
  member of the complete module set changes.
- Lifecycle, mode, and heartbeat publications carry the title generation.
  Stale or foreign generations are rejected. A heartbeat must be strictly
  newer than both the title-generation boundary and the complete module-set
  boundary.
- On the first transition from one title module to a multi-resident set, the
  already-hooked title gets a bounded 100 ms teardown-only pending interval to
  publish a post-transition heartbeat. Pending exposes no armed state,
  heartbeat, gameplay mode, or capabilities; it never installs a hook, admits
  a different title, or survives multiple qualifying owners.
- New H3 or ODST hooks are installed only when exactly one title module is
  available. This candidate deliberately does not solve entry into an unhooked
  new title while an old title DLL remains resident; that remains a later,
  separately tested resident-module lifecycle candidate.
- Halo 3 ownership uses the accepted strict `<500 ms` camera-fresh boundary.
  ODST preserves its accepted asymmetric lifecycle: `<500 ms` drives fresh
  camera debounce, an unready camera falls back after `>750 ms`, and a
  previously seen still-ready camera is retained through age 5000 ms and falls
  back at 5001 ms. Multi-resident ownership is always clamped to strict
  `<100 ms` for both titles.
- Unarmed owners can expose only capabilities that do not require the armed
  camera transaction. Stereo, aim, HUD, arm IK, room scale, and haptics are
  masked until armed. Ordinary controller input and runtime-mode reporting are
  separate capabilities.

## Publication sites and safety

- Halo 3 publishes only at the existing `CamCopyHook` camera-transform point.
- ODST publishes only after the original native camera copy, inside the
  existing proven slot-0/single-user/active-camera branch.
- The hot publication path is fixed-storage, bounded, atomic, lock-free,
  allocation-free, log-free, and does not scan or perform file I/O.
- ODST teardown still disables the outer renderer first, lets an in-flight
  two-eye transaction finish, verifies callback quiescence, removes every hook,
  restores native patches and variables, and only then clears installed state.
  Incomplete cleanup remains installed with teardown requested and zero exposed
  capabilities.
- Reach publishes no generation-tagged lifecycle, mode, heartbeat, owner, or
  capability state.
  `TitleHookPlan::None` and `ReachAdapter_RuntimeHooksPermitted()==false` remain
  unchanged in both presets.

## Desk validation and required headset regression

The deterministic core matrix covers generation rollover, exact epoch and
freshness boundaries, future/stale/foreign publications, zero/one/multiple
owners, pending grace, teardown, mode invalidation, heartbeat clearing,
capability masking, and Reach's zero capability mask. Both cumulative Release
presets must build and pass CTest before packaging:

- Reach OFF: Halo 3 + ODST, Reach stock.
- Reach ON: Halo 3 + ODST plus the inert Reach evidence adapter.

Desk tests cannot accept this shared lifecycle change. The exact packaged DLL
hash still requires, at minimum, an ODST headset result and a Halo 3 regression
covering level entry/exit, pause, Save & Quit, fresh/stale camera transitions,
native HUD and weapon stereo, controller input/aim, movement, and rumble. Until
those results are recorded, 0.2.2 remains the only accepted build.
