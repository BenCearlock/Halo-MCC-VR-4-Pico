# Halo: Reach signature evidence

Status: **preliminary evidence only; no Reach runtime hook is authorized**.

This document records Reach-specific facts for the cumulative Halo 3 + ODST +
Reach line. Halo 3 and ODST offsets, layouts, bones, markers, and tag meanings
are not Reach evidence. The authoritative accepted baseline remains public
release `MCC_VR_ALPHA_0.2.2`, runtime source
`3a2a11bfc66b36e70f60282e91c9d5436f2e18d1`.

## Pinned inputs

The installed Steam retail module was verified read-only on 2026-07-23:

| Field | Value |
| --- | --- |
| Module | `haloreach.dll` |
| SHA-256 | `738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894` |
| PE timestamp | `0x68A0EFE1` |
| `SizeOfImage` | `0x04EDA000` |

The official installed Halo: Reach Editing Kit is build
`2023.07.17.176677.1-QFE1`. Its `HREK.7z` was extracted only into the installed
`HREK` directory after confirming `tags/` and `data/` were absent. Kit and game
assets are local evidence only and must never be committed or packaged.

The machine-readable copy of these identities is
`docs/REACH-EVIDENCE-MANIFEST.json`.

## Preliminary function candidates

The names below are hypotheses and call-graph hints, not established ABIs or
hook sites. Offline PE inspection confirms only that their RVAs fall inside the
retail file's executable `.text` section. That does not prove a unique loaded
image signature or any function meaning.

| Candidate | RVA | Loaded-image unique | Executable range | ABI | Callers | Data flow | HREK semantics | Layout fields | Status |
| --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- |
| Viewport | `0x287F58` | No | Offline file only | No | No | No | No | No | Unproven |
| First-person camera upload | `0x282D60` | No | Offline file only | No | No | No | No | No | Unproven |
| Visible palette | `0x2B4EB0` | No | Offline file only | No | No | No | No | No | Unproven |
| Special-bone composer | `0x213224` | No | Offline file only | No | No | No | No | No | Unproven |

No candidate may become a runtime hook until all columns are independently
proven against HREK and MCC's x64 loaded image. A usable AOB must match exactly
once in the expected executable range and fail open on zero or multiple
matches. Every consumed field needs finite-value, bounds, index, and count
guards plus an understood teardown boundary.

## Foundation candidate behavior

`HALOMCCVR_EXPERIMENTAL_REACH_BRINGUP=ON` currently compiles an evidence-only
adapter shell. Reach remains `runtimeSupported=false`, advertises no
capabilities, receives `TitleHookPlan::None`, and cannot install a runtime hook.
Rendering, input, movement, aim, HUD, and lifecycle behavior remain stock.

The offline `tools/reach-preflight.ps1` command reads the retail file and HREK
identity only. It performs no injection, process attach, memory write,
protection change, debug attach, or detour.

## Evidence still required

Independently derive and document the Reach camera/view transaction, observer
effects, prepared-view stride, first-person stages, native CHUD stages,
pause/cinematic state, skeleton and weapon-marker facts, HUD anchor, brightness,
motion blur, and lifecycle boundaries before enabling player-visible behavior.
Xbox 360 map symbols may supply names or call-graph hints only; every fact must
be re-proven against HREK and the pinned MCC x64 module.
