# Building Halo MCC VR

This builds one cumulative Halo 3 + ODST + Halo: Reach runtime. Every title is a
permanent, fail-open part of the single Release build -- there is no experimental
Reach flag to toggle. Every generated file stays under ignored `out/`; nothing
writes to an MCC installation.

## Requirements

- Windows x64.
- Visual Studio 2022 with **Desktop development with C++**.
- CMake 3.24 or newer.
- Git and network access for the first dependency download.

OpenXR, MinHook, and Dear ImGui are pinned to exact commits in
`CMakeLists.txt`. Fetches are shallow and shared under `out/deps`.

## Build and test

From a Developer PowerShell:

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release
```

There is a single `release` preset. It always builds Release x64 with Halo 3,
ODST, and Halo: Reach compiled in permanently. `camscan` is excluded: it is an
opt-in diagnostic with process-memory write modes, not a product target. The
standalone Reach runtime observer is also excluded and must be selected by name;
it is never linked into `halo3xr.dll`. The build identity line reports
`ODST=ON, Reach=ON, ReachRender=ON`.

## How the permanent Reach camera core behaves

Reach is fail-open: it renders exactly as stock until its runtime proof passes.
The 50 ms title worker verifies the exact loaded Reach PE/file identity, three
unique executable signatures, both function-body hashes, six caller edges, and
fixed ranges once per sole-title admission epoch, and the Present path validates
the exact swapchain buffer 0 and builds two private per-eye caches. Only after
that proof and a one-second fresh-camera safety interval does the worker install
two hooks -- the inner `player_view_render` and outer `main_render_view` -- and
arm the per-eye stereo transaction. Any unmet precondition, an invalid camera,
or a fault falls open to a single stock render, and Halo 3 and ODST are never
touched. When Reach is armed the log reports `Reach camera core armed` and
`Reach camera bring-up: head tracking, stereo, and 6DOF ON`.

The camera-injection correctness (how the OpenXR head/eye pose maps into Reach's
camera) is a headset-tuning result, exactly as it was for the first ODST
camera-core milestone.

`TitleHookPlan::None`, zero Reach runtime capabilities, and the hard runtime-
hook gate remain unchanged. The candidate installs no Reach detour, writes no
engine memory, changes no camera, issues no Reach `CopyResource`, and cannot
select the stereo transaction. Signature scanning, file hashing, resource
allocation, and candidate logging stay off Present and all engine render
callbacks. Present performs only the bounded identity/field snapshot plus
`GetBuffer(0)`, device/descriptor capture, and COM retention described above;
eye allocation and proof publication remain on the worker.

## Verify local Reach evidence

After installing and extracting the official HREK, run the offline preflight:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\reach-preflight.ps1
```

It read-opens the configured installed retail `haloreach.dll`, HREK build tag,
and pinned `reach_tag_test.exe` evidence binary to verify their exact hashes/PE
identities and the retail module's local PE section table. It does not launch
or attach to an MCC process, inject, write memory, change page protection,
install a detour, or copy kit/game assets into the repository.

## Build and package the standalone Reach observer

The Reach observer is a diagnostics-only executable. When explicitly run, it
read-opens an already running anti-cheat-disabled MCC process with only
`PROCESS_QUERY_INFORMATION | PROCESS_VM_READ`; it does not inject, install a
hook, or write game memory. Building or packaging it does not run it.

At runtime the observer records the SHA-256 of its own running executable in
the first log line. It refuses to run from inside the MCC installation, refuses
a log path there, and resolves the process image, observer, install root, and
output parent through file handles to normalized volume-GUID paths. It then
holds the output-parent handle without delete sharing and creates the log
as a single name relative to that exact handle with `NtCreateFile`,
`FILE_CREATE`, read sharing only, and `FILE_OPEN_REPARSE_POINT`. That closes
path-alias/junction, ancestor-rename, and raced-leaf routing, gives the log one
writer, and prevents an existing log from being overwritten. Its self-hash is
also read from the already-open running executable handle, which denies
write/delete sharing and remains retained through the first log line.

To build the observer without packaging or starting MCC:

```powershell
cmake --preset release
cmake --build --preset release --target `
  reach_runtime_observer halomccvr_core_tests
ctest --preset release
```

Commit the intended diagnostic source first, then create a uniquely identified
package:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\package-reach-observer.ps1
```

The packaging command refuses a dirty worktree or a commit that does not
descend from the accepted 0.2.2 runtime source. It also requires the evidence
manifest to retain `runtime_observer.status=IMPLEMENTED_UNRUN`, an empty
`observed_runtime_results` array, false observer proof/hook fields, and false
player-view-transaction proof/hook fields. It configures the normal Release
preset, builds only the observer and core tests, runs the tests and offline
Reach evidence preflight, and rejects observer source that requests anything
other than query/read process access or names a write, injection, debugger, or
process-launch API. It also checks that the executable self-hash remains bound
to the already-open running image, the MCC path refusals use canonical handles,
and the single-writer log remains coupled to handle-relative `FILE_CREATE`.

It stages a new directory such as:

```text
out/diagnostics/1a2b3c4-reach-runtime-observer-20260723-120000000Z/
```

That directory contains `reach-runtime-observer.exe`, the observer runbook,
the license, and `DIAGNOSTIC-MANIFEST.json`. The manifest records the exact
source commit, expected retail Reach module identity, read-only process rights,
file sizes and SHA-256 hashes, observer runtime guards, and `UNRUN` status. The
command never launches or attaches to an MCC process, never runs the observer,
and never writes to the MCC installation. Its offline preflight read-opens the
configured installed `haloreach.dll` and HREK evidence. Build and dependency
outputs may be created elsewhere under ignored `out/`; package staging is
confined to `out/diagnostics`.

Do not copy the observer into MCC. When a runtime capture is explicitly
requested, keep it in its diagnostic package and follow
`REACH-RUNTIME-OBSERVER.md`; packaging alone does not advance Reach proof or
authorize hooks.

## Create a test candidate

Commit the intended source first, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\package-candidate.ps1
```

For an explicitly private Reach candidate, use:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\package-candidate.ps1 -ReachPrivate
```

For the hard-off Reach cold-proof/resource-readiness candidate, use:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\package-candidate.ps1 -ReachRenderDisabled
```

That mode additionally runs the offline Reach evidence preflight and records
separate compiled, cold loaded-image-preflight, display-copy-readiness,
`CopyResource`, engine-write, stereo-candidate, and runtime-hook fields. Only
the two cold readiness fields are true; copy, engine writes, stereo selection,
and Reach runtime hooks remain false.

Each command rebuilds and tests its selected Release preset before staging it.
The two Reach switches select controller-only or cold-proof/hooks-off candidates;
the default command retains the cumulative Reach-OFF regression gate.

The command refuses a dirty worktree, reconfigures, rebuilds, reruns tests, and
creates a new directory such as:

```text
out/candidates/1a2b3c4-reach-private-20260723-120000000Z/
```

It contains only the DLL, launcher, license, generic manual, and a
`CANDIDATE-MANIFEST.json` with the full commit, base release 0.2.2, ODST and
Reach build states, explicit `reach_controller_input_enabled` and
`reach_runtime_hooks_enabled` states, exact file sizes, and SHA-256 hashes. It
never copies to MCC, never reuses a candidate directory, and never labels
rebuilt bytes as release 0.2.2.

Deployment is manual and requires explicit user approval for that exact
candidate. A rebuild uses the accepted source/configuration but remains
unaccepted until its exact hash passes a headset test.

## Inspect the published 0.2.2 source

Use a separate clean clone or worktree so historical outputs cannot mix with
active candidates:

```powershell
git switch --detach MCC_VR_ALPHA_0.2.2
cmake --preset release
cmake --build --preset release
ctest --preset release
```

The release tag is `e2c049e5c3b98ce466f6072da4e0aa55ccc88e10`; its headset-tested
runtime source is `3a2a11bfc66b36e70f60282e91c9d5436f2e18d1`. Exact dependency
commits and published hashes are recorded in `releases/0.2.2/manifest.json`.

## Exact bytes

The exact headset-accepted DLL and launcher come from the official binary asset
`MCC_VR_ALPHA_0.2.2.zip`, SHA-256
`43E52AEF5A2D1647A8F3AE6AEFDB6C22F0C67C7AA06FD70D327FB3E00ACF5DCC`.
The ignored local exact-byte copy is `dist/MCC_VR_ALPHA_0.2.2.zip`. A local build
gets a new hash because compile time and toolchain output affect its bytes.
Never substitute a rebuild for accepted artifacts or overwrite the preserved
ZIP.
