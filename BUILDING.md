# Building Halo MCC VR

This builds the cumulative Halo 3 + ODST source from the accepted 0.2.2 line.
Reach stays stock in the normal Release preset. Every generated file stays
under ignored `out/`; nothing writes to an MCC installation.

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

The preset always builds Release x64 with ODST enabled and Reach disabled.
`camscan` is excluded: it is an opt-in diagnostic with process-memory write
modes, not a product target.

During private Reach bring-up, validate both isolated build trees:

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release

cmake --preset release-reach-private
cmake --build --preset release-reach-private
ctest --preset release-reach-private
```

The private preset retains Halo 3 and ODST and compiles the Reach evidence
adapter. Candidate one remains structurally inert: it installs no Reach hooks
and leaves Reach rendering, input, movement, aim, and HUD stock.

## Verify local Reach evidence

After installing and extracting the official HREK, run the offline preflight:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\reach-preflight.ps1
```

It only reads the pinned retail `haloreach.dll`, HREK build identity, and local
PE section table. It does not attach to MCC, inject, write memory, change page
protection, install a detour, or copy kit/game assets into the repository.

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

That command rebuilds and tests both the Reach-OFF and Reach-ON Release trees
before staging only the private tree.

The command refuses a dirty worktree, reconfigures, rebuilds, reruns tests, and
creates a new directory such as:

```text
out/candidates/1a2b3c4-reach-private-20260723-120000000Z/
```

It contains only the DLL, launcher, license, generic manual, and a
`CANDIDATE-MANIFEST.json` with the full commit, base release 0.2.2, ODST and
Reach build states, explicit Reach-hook state, exact file sizes, and SHA-256
hashes. It never copies to MCC, never reuses a candidate directory, and never
labels rebuilt bytes as release 0.2.2.

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
