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

The HREK executable used for the camera/frustum corroboration below is
`reach_tag_test.exe`, SHA-256
`CBDD8448A87A433B0DFFC0DE47D06DB7A18B4BF868B96B057135DAA86790ABA8`,
PE timestamp `0x64B47B50`, `SizeOfImage` `0x06247000`. The hash records the
exact local evidence input; the executable is not stored in this repository.

The machine-readable copy of these identities is
`docs/REACH-EVIDENCE-MANIFEST.json`.

## Preliminary function candidates

Unless a row says otherwise, the names below remain hypotheses and call-graph
hints rather than established ABIs or hook sites. The original foundation
proved only that all four RVAs fall inside the retail file's executable `.text`
section. The frustum-bounds entry has since passed the additional static gates
recorded below; the other three have not.

| Candidate | RVA | Loaded-image unique | Executable range | ABI | Callers | Data flow | HREK semantics | Layout fields | Status |
| --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- |
| Camera-derived frustum bounds (original viewport hypothesis) | `0x287F58` | Exact stock observer passed the unique loaded-image check twice; production must repeat it | Yes, `.text`, `0x287F58`-`0x2881CC` | Yes | Nine direct callers | Yes | Yes | Static consumers and producer proven; live lifetime still open | Static role proven; hook forbidden |
| First-person camera upload | `0x282D60` | No | Offline file only | No | No | No | No | No | Unproven |
| Visible palette | `0x2B4EB0` | No | Offline file only | No | No | No | No | No | Unproven |
| Special-bone composer | `0x213224` | No | Offline file only | No | No | No | No | No | Unproven |

No candidate may become a runtime hook until all columns are independently
proven against HREK and MCC's x64 loaded image. A usable AOB must match exactly
once in the expected executable range and fail open on zero or multiple
matches. Every consumed field needs finite-value, bounds, index, and count
guards plus an understood teardown boundary.

## Camera-derived frustum-bounds proof

The original `viewport` hypothesis is now statically identified as the
camera-derived frustum-bounds helper. This is one building block in Reach's
camera path, not a safe stereo transaction boundary.

### Retail identity, signature, and boundary

The existing production `kBuildViewportSig` is:

```text
40 53 48 83 EC 30 44 0F BF 49 62 4C 8B D9 4C 8B 41 38 48 8B DA 0F BF 51 50
```

On the pinned Reach module it matches exactly once at RVA `0x287F58` when
scanned across executable sections in a Windows
`PAGE_READONLY | SEC_IMAGE_NO_EXECUTE` image mapping. The mapping executes no
code and does not load or attach to MCC. PE exception metadata bounds the
function at `0x287F58`-`0x2881CC`: 628 bytes and 154 instructions. No base
relocation overlaps the AOB or the complete function.

The complete relocation-normalized instruction sequence is identical to the
accepted Halo 3 helper at `0x2A63E4` and the accepted ODST helper at
`0x2CAC5C`. The comparison retained mnemonics, registers, structure
displacements, and non-control-flow constants and ignored only resolved
RIP-relative addresses and branch/call destinations. This cross-title match is
corroboration only; the Reach retail and HREK evidence below establish the
Reach-specific ABI and field meanings.

### ABI, callers, and data flow

The Windows x64 ABI is:

- `RCX`: compact camera input;
- `RDX`: writable four-float frustum-bounds output;
- `AL`: `true` on return.

The retail helper is a leaf with no direct or tail callees. Its nine direct
call sites are `0x1D2F4C`, `0x1D3594`, `0x1D3C4D`, `0x25B47C`, `0x25D41A`,
`0x26C2FF`, `0x26FB1B`, `0x286DD8`, and `0x28DB6D`. At every site, the next
direct call is the projection builder at `0x2884BC`, with the same camera
pointer and the four derived floats. This proves the output's downstream role,
but the nine-call-site fan-out also proves this helper is shared and cannot by
itself identify the player-eye render transaction.

The first-person path is one of those callers. Inside retail function
`0x286C6C`-`0x286EBC`, it calls the frustum helper at `0x286DD8`, calls the
projection builder with the same camera/bounds at `0x286DEF`, then tail-jumps
to the preliminary camera-upload candidate `0x282D60` at `0x286E6A`. This
establishes ordering evidence for later first-person work; it does not yet
authorize either hook.

### HREK semantic corroboration

In the pinned HREK `reach_tag_test.exe`, the homologous function is
`0x823930`-`0x823D16`. Its retained assertions identify:

- `RCX` as `camera` and `RDX` as `frustum_bounds`, from
  `render/render_cameras.cpp` lines 443-444;
- the ordered `camera->render_pixel_bounds` x and y ranges at source lines
  451-452.

It performs the same field reads, scaling, four-float writes, optional custom
projection adjustment, and `true` return as the retail function. This is
independent title-specific semantic evidence; the editing-kit executable is
not byte-compatible with the MCC DLL and is not an AOB-count target.

### Consumed compact-camera fields

Reach rectangles use signed 16-bit `y0,x0,y1,x1` order. Retail initialization
at `0x287D60`-`0x287DF9` and the HREK assertions establish the following
consumers and producers:

| Offset | Static Reach role |
| ---: | --- |
| `+0x38..+0x3F` | `window_pixel_bounds`; initialized from `+0x4C` and consumed as the active window rectangle |
| `+0x40..+0x47` | 5%-95% inset/title-safe rectangle copied from `+0x54`; exact member spelling remains open |
| `+0x4C..+0x53` | `render_pixel_bounds`; initialized to zero origin plus current render extents and used as the scaling denominator |
| `+0x54..+0x5B` | 5%-95% inset rectangle; copied to `+0x40` by the initializer |
| `+0x5C..+0x63` | current client/display bounds; producer `0x24F48C` reads `GetClientRect`, applies an eight-pixel minimum, and writes zero origin plus bottom/right extents |
| `+0x7C` | custom projection-window enable |
| `+0x80`, `+0x84` | custom horizontal and vertical center offsets |
| `+0x88`, `+0x8C` | custom horizontal and vertical extent scales |

The helper writes exactly four floats at output offsets `+0x00..+0x0F`.
Retail constants used by the centering/scaling math are `0.5f` and `2.0f`.

### Remaining gate

The exact external stock observer repeated the exactly-one scan and loaded
range checks successfully in two admitted sessions. Any future production
runtime must independently repeat that cold preflight and fail open on a
mismatch. The player-view transaction proof below still does not make this
shared helper a safe hook. Caller scope, hook-time camera/workspace lifetime
and full snapshot/restore, live capture-target lifetime, production
finite/range guards, teardown, and headset validation remain required.
Therefore `proof_complete` and `hook_eligible` remain false.

## Player-view prepare/render transaction

The stock player-view owner, object stride, same-call freshness, late native
CHUD order, and transaction-scoped active-view clear are now statically
identified. This closes the corresponding static questions only. Stock Reach
has output-user/player-view transactions, not VR-eye transactions, and no
Reach hook or eye capture exists yet.

### Retail owner and boundary

Retail `main_render_game`, bounded by PE exception metadata at
`0x0C33F8`-`0x0C3B1E`, owns the normal player-window loop. For each active
entry it:

1. indexes the player-view array;
2. calls setup `0x26C204`-`0x26C6DC` at `0x0C36D6`;
3. writes the last-window flag at player-view `+0xA30`;
4. calls `main_render_view` `0x0C31F4`-`0x0C33F7` at `0x0C3730`.

The `main_render_view` ABI is `RCX = rasterizer-camera workspace`,
`RDX = player_view*`, and `R8D = player-window index`. The normal caller passes
workspace RVA `0x00C9FAE0`. The function creates frame textures, computes
visibility, and calls `player_view_render` `0x26C6DC`-`0x26CFE6` at
`0x0C33C4`.

Its complete body SHA-256 is
`95DF3EFFF9AC6EE29887D1272CCA8D7BF3E58F87041BAD8032107825B733FE89`.
The following exact 32-byte entry occurs once in the pinned retail `.text`
bytes:

```text
40 53 56 57 48 81 EC 80 00 00 00 0F 29 74 24 70
48 8B 05 05 6E A3 00 48 33 C4 48 89 44 24 68 41
```

That is offline identity evidence, not loaded-image authorization. A second
direct caller at `0x1D3864`, in `0x1D3784`-`0x1D3892`, passes a camera at
`[RSI+8]`, a view at `[RSI+0x10]`, and index zero. Its screenshot/alternate
render semantics are not yet fully resolved, so future routing must prove
caller scope rather than treating every `main_render_view` call as gameplay.

### Four player views and the `0xA40` stride

The retail player-view array starts at RVA `0x029F2B90`. Static initializer
`0x6210`-`0x6246` constructs exactly four objects by calling
`0x25B0A8` and advancing `0xA40` at `0x622E`. Independently, the live render
loop multiplies the active index by `0xA40` at `0x0C366A`, passes that object to
setup, then advances its current-view pointer by `0xA40` at `0x0C3879`.

The pinned HREK independently repeats both facts:

- array RVA `0x04D8AED0`;
- initializer `0x20AA0`-`0x20AD6`, four calls to constructor `0x833660`,
  with the `0xA40` increment at `0x20ABE`;
- runtime index multiply at `0x1CBC80` and loop increment at `0x1CBF65`;
- setup call `0x1CBD3A -> 0x837F80`;
- render call `0x1CBF22 -> 0x834490`.

Thus `0xA40` is the Reach player-view object stride and extent. It is **not**
permission to transplant a Halo 3 or ODST prepared-view layout. Reach
player-view `+0x08` is an observer/state pointer, while the compact and derived
cameras live in the separate rasterizer workspace.

### Same-transaction freshness and active-view lifetime

Retail setup materializes the default workspace at RVA `0x00C9FAE0` in the
same player-loop iteration. It initializes the camera, calls the proven
frustum helper at `0x26C2FF`, calls the projection builder at `0x26C316`, and
copies:

- compact camera `+0x000..+0x08F` to secondary `+0x154..+0x1E3`;
- derived block `+0x090..+0x153` to secondary `+0x1E4..+0x2A7`.

The owner calls setup at `0x0C36D6` and `main_render_view` at `0x0C3730`
without a loop or frame boundary between them. `main_render_view` stores the
current `player_view*` in global RVA `0x04E389A8` at `0x0C323C`, renders it,
runs paired end calls at `0x0C33C9` and `0x0C33CE`, then clears the global at
`0x0C33D3`.

HREK supplies the semantic proof. Inside `main_render_game`
`0x1CB8D0`-`0x1CC001`, the retained `main_render_view` profile envelope is
`[0x1CBE36,0x1CBF51)`. It sets the active view at
`0x1CBE89 -> 0x837B70`, creates frame textures, computes visibility, calls
source-named `player_view_render` at `0x1CBF22 -> 0x834490`, and clears the
active view with null at `0x1CBF33 -> 0x837B70`. Retained
`render_player_view.cpp` records at lines 2504, 2505, 2507, and 2508 name the
stored value `g_player_view_stack_element` and validate both render-camera and
rasterizer-camera position/up/forward vectors.

This proves fresh setup relative to the immediately following stock
transaction and proves its engine-owned set/use/clear bracket. It does not
prove a continuously fresh camera heartbeat, the required one-second safety
interval, or transition/device-loss/thread teardown.

### World, first-person, native CHUD, capture, and Present

Reach does not have a strict low-level `world -> weapon` total order. World,
effects, and first-person subpasses are interleaved inside
`player_view_render`; claiming otherwise would be false. The following weaker
and sufficient stock ordering is proven:

| Phase | Retail evidence | HREK semantic evidence |
| --- | --- | --- |
| Player world/view transaction begins | `0x0C33C4 -> 0x26C6DC` | `0x1CBF22 -> 0x834490`, source-named `player_view_render` |
| First-person stages | `0x26CAB6 -> 0x26DA08`; final wrappers at `0x26CBD5` and `0x26CDC0 -> 0x26EA78`; camera rebuild at `0x26EC3B`/`0x26EC7E -> 0x286C6C` | Retained `first_person_pass` label and source assertions |
| Late native CHUD | `0x26CE28 -> 0x2C279C`, then `0x26CEC9 -> 0x2C29F8` | `0x8354AE -> 0x8B67C0` is source-named `chud_draw_screen_LDR`; `0x8354E5 -> 0x8355A0` reaches source-named `chud_draw_screen` at `0x835682 -> 0x8B6300` |
| Post-render continuation | `0x0C33C9`, after every phase that executed | Return from source-named `player_view_render` |
| Stock Present | outer frame function `0x0C2DFC`-`0x0C3033` calls main render at `0x0C2FAA`, then `0x0C3000 -> 0x25113C`; Present vcall `+0x40` is at `0x2511AA` | `0x1CB66A -> 0x1CB8D0`, then `0x1CB810 -> 0x7C5F10`, retained `swap_chain_present` |

The first-person and both late CHUD paths are conditional. CFG branches can
skip them, but every path that executes the final first-person work reaches it
before either late CHUD phase. No backward edge after the final first-person
region reverses that order.

The exact in-scope post-render continuation therefore begins at `0x0C33C9`:
after the stock player renderer, including its executed native CHUD, and before
the active-view clear. A future detour around
`main_render_view` can instead capture post-original; that is still per-view
and before stock Present, but it is after the engine has cleared its active
pointer. The two placements have the same phase order but not the same
lifetime proof.

Static analysis proves placement, not a usable OpenXR capture target or a
successful copy. It also proves why a CHUD callee alone is unsafe:
`0x1D3894`-`0x1D3E27` separately calls `0x2C29F8` at `0x1D3D24` for an
alternate/screenshot path.

### Remaining runtime gate

The static owner, stride, within-call freshness, late-CHUD order, pre-Present
capture placement, and transaction-scoped clear are closed. Before a Reach
hook is eligible, the remaining runtime-evidence and implementation gates are:

- normal caller scope and alternate-caller semantics, while production repeats
  the now-corroborated exactly-one loaded-image checks;
- production enforcement of the now-observed continuous camera freshness and
  one-second safety interval;
- the render target that remains valid at the selected capture point;
- pause, cinematic, split-screen, unload/reload, device-loss, and title-module
  transition behavior;
- callback quiescence and complete detour teardown;
- finite-value, range, index, count, and failure-to-stock guards;
- exact-DLL headset validation plus Halo 3 regression.

Accordingly, all Reach `proof_complete` and `hook_eligible` fields remain
false.

## Controller-input-only candidate behavior

`HALOMCCVR_EXPERIMENTAL_REACH_BRINGUP=ON` now compiles a
controller-input-only adapter. When module resolution identifies explicit
Reach, it grants only shared virtual-controller admission. Reach remains
`runtimeSupported=false`, its runtime capability mask remains
`TitleCapability_None`, it receives `TitleHookPlan::None`, and
`ReachAdapter_RuntimeHooksPermitted()` remains false.

This admission carries ordinary virtual XInput buttons and sticks into stock
Reach. It does not grant a Reach runtime owner or enable camera or render
changes, controller-aim or movement transforms, HUD, arm IK, haptics, runtime
mode publication, lifecycle publication, or any runtime hook. A
multi-resident `Unknown` frontend cannot claim the Reach-specific admission
because it cannot be identified as explicit Reach. Any title-independent
frontend controller-continuity fallback is separate and is not Reach gameplay
support.

The offline `tools/reach-preflight.ps1` command reads the retail file and HREK
identity only. It performs no injection, process attach, memory write,
protection change, debug attach, or detour.

## Standalone runtime observer

Status: **OBSERVATIONS_REVIEWED_INCOMPLETE**.
`reach-runtime-observer.exe` is an external read-only diagnostic, not an
injected Reach candidate. It requests only
`PROCESS_QUERY_INFORMATION | PROCESS_VM_READ`, installs no hook or detour,
injects no code, and writes no process memory or MCC file. Its only file write
is a new, exclusively created self log; it never overwrites an existing log.
It must be run against stock anti-cheat-disabled MCC and refuses a process with
`halo3xr.dll` loaded. It resolves the process image, observer executable,
derived installation root, and output parent through file handles to normalized
volume-GUID paths before refusing either its executable or output inside MCC.
The output-parent handle remains open without delete sharing and is the
`RootDirectory` for a single-component `NtCreateFile` log open using the
`FILE_CREATE` disposition and `FILE_OPEN_REPARSE_POINT` option, so aliases,
symlinks, junctions, ancestor renames, a raced leaf reparse point, and an
existing path cannot bypass the refusal. The observer hashes its already-open
running-image file handle, denies write/delete sharing on that handle, and
retains it through the first log line.

Before sampling, it verifies the pinned backing-file hash, loaded PE identity,
exactly-one executable-section matches at the expected `main_render_view` and
frustum RVAs, the complete `main_render_view` body hash, six proven `rel32`
edges, and committed readable `MEM_IMAGE` ownership for the fixed data ranges.
Its final snapshot rechecks the same Reach mapping, absence of `halo3xr.dll`,
and Reach as the sole resident title module. It re-runs that preflight after
every observed Reach unload/reload and pauses when another title module makes
ownership ambiguous. Because module state is polled every 100 ms, it can miss
a complete unload/reload between polls when the replacement has the same base
and mapping identity; only logged reloads are corroborated.

The observer samples active-view global RVA `0x04E389A8` and accepts only exact
starts of the four slots at RVA `0x029F2B90` with stride `0xA40`. A new session
or continuity reset must witness a clear value before any non-null value can
count, so attaching mid-transaction cannot seed evidence. For each later
observed normal-slot transition, it uses a pointer/workspace/pointer read to
discard torn snapshots and validates both the primary Reach compact camera at
workspace `+0x000` and the secondary compact camera at `+0x154`. Both must pass
before the sample is usable; byte equality is counted but is not required. The
runtime guards cover finite position/forward/up, normalized and orthogonal
axes, vertical FOV, ordered signed `y0,x0,y1,x1` bounds, and the proven
zero-origin/eight-pixel-minimum client bounds. A nonzero observed slot can
runtime-corroborate the static `0xA40` spacing; slot 0 alone corroborates only
the array base and is insufficient runtime evidence for the stride.

Its per-slot freshness ledger treats a sampled transaction as fresh for less
than 500 ms and reports a stable observation only when exactly one slot is
fresh and that slot has at least two usable transactions whose last-minus-first
span is strictly greater than 1000 ms, without a 500 ms gap. Two simultaneously
fresh slots fail closed and reset every slot window; interleaved split-screen
work cannot establish ownership. This is observer evidence only and never arms
VR. Polling can miss the short engine-owned active-view pulse, so absence is
inconclusive. A preflight-only run exits nonzero and is likewise inconclusive.

The exact source-`5d34180` executable, SHA-256
`AC43FA4F65256DF1CB46B9C0471DDA97E3120265AEDC876E0A3A73FC6A86CF6A`,
was run against stock MCC for 480,000 ms. Its retained log at
`out/test-runs/5d34180-stock-reach-observer-20260724-025036448Z/reach-runtime-observer.log`
has SHA-256
`3C36AF1F06FC428E914AB0C71330838587B020335EFBF2B017F8EF178768212D`.
Two preflights passed; 29,507 normal slot-0 transactions yielded 29,496 valid
camera samples and seven stable windows, with no invalid cameras,
outside-array pointers, multi-owner intervals, contamination, or snapshot
failures. The observer safely reset on ambiguous residency, re-ran preflight
when Reach became sole-resident again, and later recorded one Reach
unload/title exit. Only slot 0 appeared, so the array base is
runtime-corroborated but the `0xA40` stride is not.

The complete safety contract, procedure, exact counters, and interpretation
limits are in `docs/REACH-RUNTIME-OBSERVER.md`. The pass runtime-corroborates
the pinned loaded image and observed single-owner camera freshness only. The
tool cannot prove exact caller execution or alternate-caller semantics, GPU
target/copy lifetime, split-screen, an unload/reload pair, device-loss or
detour teardown, stereo-eye behavior, headset parity, or a player-visible
Reach path. `proof_complete` and `hook_eligible` remain false.

## Evidence still required

Resolve the normal and alternate caller scopes and the owning capture target.
Prove hook-time camera/workspace lifetime and exact snapshot/restore,
production freshness enforcement, pause/cinematic/split-screen behavior,
unload/reload and device-loss handling, callback quiescence, and complete
detour teardown. Still independently derive observer effects, stereo camera
mutation, first-person weapon behavior, HUD anchor, skeleton and weapon-marker
facts, brightness, and motion blur before enabling player-visible behavior.
Xbox 360 map symbols may supply names or call-graph hints only; every fact must
be re-proven against HREK and the pinned MCC x64 module.
