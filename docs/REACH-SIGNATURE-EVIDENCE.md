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
| Camera-derived frustum bounds (original viewport hypothesis) | `0x287F58` | Stock observer found the 24-byte prefix unique twice; canonical byte 25 was not rerun | Yes, `.text`, `0x287F58`-`0x2881CC` | Yes | Nine direct callers | Yes | Yes | Static consumers and producer proven; live lifetime still open | Static role proven; hook forbidden |
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

The exact external stock observer found the first 24 bytes unique at the
expected RVA and repeated the loaded-range checks in two admitted sessions. It
did not check canonical byte 25. Any future production runtime must scan all 25
bytes, require exactly one match at the expected RVA, and fail open on a
mismatch. The player-view transaction proof below still does not make this
shared helper a safe hook. Caller scope, hook-time camera/workspace lifetime
and full snapshot/restore, live capture-target lifetime, production finite/range
guards, teardown, and headset validation remain required.
Therefore `proof_complete` and `hook_eligible` remain false.

## Player-view prepare/render transaction

The stock player-view owner, object stride, same-call freshness, exact normal
and screenshot caller scopes, camera-workspace lifetime, final display target,
late native-CHUD order, and transaction-scoped active-view clear are now
statically identified. This closes the corresponding static questions only.
Stock Reach has output-user/player-view transactions, not VR-eye transactions,
and no Reach hook or eye capture exists yet.

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

That is offline identity evidence, not loaded-image authorization. The pinned
retail image has exactly two direct rel32 callers:

- normal player rendering at `0x0C3730`, return RVA `0x0C3735`;
- screenshot rendering at `0x1D3864`, return RVA `0x1D3869`.

The second call is inside helper `0x1D3784`-`0x1D3892`. It passes workspace
`[RSI+8]`, player view `[RSI+0x10]`, and index zero, so parameter checks cannot
distinguish it from a normal slot-0 transaction by ABI/index alone. Its owners are Reach's
screenshot dispatcher and high-resolution tile/bloom loops. The pinned HREK
independently labels the matching paths `screenshot bloom`, `screenshot tile`,
`render screenshot`, and `display tile`; its corresponding calls to
source-named `main_render_view` `0x1CC690` are at `0x50AC49` and `0x50D481`.
The retained `main_screenshot.cpp` warning that multi-view screenshots capture
only the first player explains the hard-coded index zero.

Future production routing must therefore whitelist exact return RVA
`0x0C3735`. Return RVA `0x1D3869` and every unknown caller must execute the
stock original exactly once, without camera mutation, capture, or stereo
duplication. This resolves the alternate caller's semantics; implementing and
validating that routing remains a hook gate.

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

The owner calls setup at `0x0C36D6`. From setup's return at `0x0C36DB` to the
`main_render_view` call at `0x0C3730` there are exactly 16 instructions and no
call or branch: four viewport-global writes, the last-window byte write at
player view `+0xA30`, and argument setup. This proves the hook entry receives
the same-iteration workspace. The one global workspace is reused for every
player iteration, so no future eye path may retain it beyond the synchronous
normal render call.

The camera-only workspace occupies `+0x000..+0x2A7`, but the exact render-scope
snapshot is `0x2B0` bytes:

| Offset | Size | Static role |
| ---: | ---: | --- |
| `+0x000` | `0x90` | primary/rasterizer compact camera |
| `+0x090` | `0xC4` | primary derived block |
| `+0x154` | `0x90` | secondary/render compact camera |
| `+0x1E4` | `0xC4` | secondary derived block |
| `+0x2A8` | `0x08` | camera-stack callback written by the render-scope push |

HREK independently repeats that `0x2B0` layout at
`0x04D86C10`-`0x04D86EC0`, and retained assertions name the primary and
secondary members `get_rasterizer_camera()` and `get_render_camera()`.
Normal setup mirrors both compact cameras and both derived blocks. The
screenshot path proves a second intentional policy: it retains the base
secondary compact camera while applying a custom projection to the primary,
then mirrors the rebuilt derived block. Static evidence therefore does not yet
choose the correct secondary-camera policy for OpenXR; every block still has
to be accounted for explicitly.

`main_render_view` stores the current `player_view*` in global RVA
`0x04E389A8` at `0x0C323C`, pushes the workspace/callback scope at
`0x0C3246 -> 0x251C08`, renders at `0x0C33C4`, begins its post-render
continuation at `0x0C33C9`, pops the camera scope at
`0x0C33CE -> 0x251C50`, then clears the active view at `0x0C33D3`. The normal
CFG has no return that bypasses the paired pop and clear after the set.

The push is a bounded stack, not an unconditional assignment. Retail depth is
RVA `0x00B43ABC`, pointer slots begin at RVA `0x00C878A8`, and valid pushed
depths are 0 through 3. `0x251C08` silently skips the push when the prior depth
is already at least 3. The exact normal call supplies callback
`module+0x0026BFD4`, which a successful push stores at workspace `+0x2A8`.
The outer token must capture the pre-push depth. Production inner admission
must require the active-view global to equal the candidate `player_view*`,
current depth to equal captured pre-push depth plus one and remain at most 3,
the new top slot to point to the admitted normal workspace, and its callback
qword to equal `module+0x0026BFD4`. This detects a silently skipped push even
if a prior top slot happens to alias the workspace. Overflow, nested-owner,
callback, depth, or top-pointer mismatch remains stock-once.

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
authorize a whole-function double call. For slot zero, `main_render_view`
executes a frame-once block at `0x0C3251`-`0x0C3296`; calling the whole function
twice would execute that block twice. It would also pop and clear the
engine-owned active scope after the first call. A future stereo boundary must
stay inside the existing scope and account for all transitive player-view and
global side effects. The observed continuous heartbeat and one-second interval
must still be enforced by production, and transition/device-loss/thread
teardown remain open.

### Inner stereo candidate and coherent rebuild constraints

PE exception metadata bounds retail `player_view_render` at
`0x26C6DC`-`0x26CFE6`: `0x90A` bytes, with body SHA-256
`2628D1189621EACED7C95A1F295815D70E7783054F1C3CBA46799F838CC33C60`.
The following entry AOB matches exactly once in the pinned retail executable;
only the RIP-relative security-cookie displacement is wildcarded, and no base
relocation overlaps it:

```text
48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55
41 54 41 55 41 56 41 57 48 8D A8 28 FF FF FF 48 81
EC B0 01 00 00 0F 29 70 C8 0F 29 78 B8 48 8B 05
?? ?? ?? ?? 48 33 C4 48 89 85 80 00 00 00 8B 81
A4 03 00 00
```

Its ABI is `void __fastcall player_view_render(player_view*)`. Retail has
exactly one direct rel32 caller, `0x0C33C4`; the HREK source-named homolog is
`0x834490`-`0x835598`. HREK has exactly two direct calls to that homolog: normal
`0x1CBF22` and alternate `0x1CC790`. An inner loop here structurally avoids
repeating the slot-zero block, frame-texture/visibility preparation,
active-view set, and camera-stack push. Stock post-render, pop, and clear still
execute exactly once after the hook returns.

This inner call cannot determine outer ownership by its own immediate return:
normal `0x0C3730` and screenshot `0x1D3864` both funnel through the same
`0x0C33C4` call. Production must validate the exact outer normal return and
propagate a bounded TLS owner token into this inner scope. A screenshot,
unknown, nested, or token-mismatched call must remain stock-once.

The stock-observed pre-scope camera rebuild sequence is:

1. mutate the primary compact camera;
2. call frustum helper `0x287F58` with `RCX = compact`, `RDX = float[4]`;
3. call projection builder `0x2884BC` with `RCX = compact`,
   `RDX = float[4]`, `R8 = primary derived`, and `XMM3 = 0.0f`;
4. explicitly make the required primary/secondary compact and derived blocks
   coherent;
5. call camera-state updater `0x286F9C` with
   `RCX = player_view+0x3B0`, `RDX = primary compact`;
6. call projection/matrix builder `0x28AF8C` with
   `RCX = player_view+0x490`, `RDX = primary derived`,
   `R8 = primary derived+0x78`, `R9 = compact+0x4C`, and stack argument 5
   pointing to the zero projection-offset pair at `player_view+0x470`;
7. enter the active-view/camera-stack render scope.

Normal setup and the independent retail/HREK screenshot paths corroborate
that ordering. The projection builder writes exactly `0xC4` derived bytes.
The camera-state updater sparsely touches a `0xC8` history/state envelope at
`player_view+0x3B0..+0x477`, including zeroing the projection-offset pair, and
shifts its own history before writing the current camera. The projection/matrix
builder's final direct output byte is `player_view+0x750`;
stock treats `player_view+0x490..+0x75F` as one `0x2D0` current-matrix block.
Setup argument 5 is separately proven by an HREK assertion to be
`output_user_index`; it is not an eye selector. The retail camera-state updater
has only the two arguments above; HREK's extra updater arguments must not be
transplanted into MCC.

If that helper subset is used, its known touched/replay units are:

- workspace `0x00C9FAE0..0x00C9FD8F`, size `0x2B0`;
- player view `+0x3B0..+0x477`, size `0xC8`;
- player view `+0x490..+0x75F`, size `0x2D0`;
- player view `+0x760..+0xA2F`, size `0x2D0`, only if the previous-matrix
  mirror is touched;
- render-camera pointer global RVA `0x04E38A90`, validated as
  `player_view+0x3B0` before admission and re-armed by each eye updater.

The render-camera pointer is not a final rollback byte. Each stock
`player_view_render` clears it at `0x26CE2F`; the final eye's stock zero must
persist. Blindly restoring the inner-entry nonzero value would create a stale
owner, while restoring an arbitrary prior value could clobber a nested owner.
Any inter-eye re-arm must come from the proven updater under a serialized owner
token and fail open on an unexpected value.

The whole `0xA40` player view must not be snapshotted or blindly restored.
The owner writes `player_view+0xA30` before rendering. A preceding conditional
at retail `0x26CB54`-`0x26CB5B` can bypass the flag block entirely. When that
block executes, `player_view_render` tests and clears the flag at
`0x26CB73`-`0x26CB7C`; HREK identifies its work as `wait_for_gpu`. The first
candidate policy is `stock_last_window && final_eye`: the first eye receives
false and the final eye receives the original stock byte. The stock original
then decides whether the block executes and consumes the byte. The final eye's
actual post-call byte must persist; it must not be assumed cleared. Static
evidence proves the conditional consumption semantics, not this policy's
runtime/headset correctness.

All proven normal and screenshot rebuilds execute before active-view set and
camera-stack push. The helper inputs still exist at the inner candidate, but
static evidence does not prove that invoking them inside the active scope is
safe. Per-eye serial reuse, temporal/previous-matrix policy, conservative
visibility for both eyes, secondary render-camera policy, OpenXR pose/unit/FOV
mapping, exception cleanup, and headset behavior therefore remain runtime
gates. This section selects the exact inner candidate; it does not make it
hook-eligible.

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

Reach's final stock target identity is nevertheless statically resolved.
Surface group 1 is constructed by `0x266F90`-`0x2670FB`: at
`0x266FC2` it reads exact swapchain global RVA `0x04E38868`, calls
`IDXGISwapChain::GetBuffer(0, ...)` at `0x266FE0`, and creates the group's
resource views.
The group-1 descriptor is RVA `0x00BB9288`; its runtime group begins at RVA
`0x00C8E520`. Its byte-identical retail/HREK descriptor specifies flags
`0x21`, `DXGI_FORMAT_R8G8B8A8_UNORM`, full-resolution scales, and one sample.
Flag `0x20` creates four `0x88`-byte specializations. The runtime group records
their count at `+0x58` and array pointer at `+0x60`; record 0 holds the
swapchain RTV at `+0x08` and SRV at `+0x18`. Creation calls
`CreateRenderTargetView(texture, nullptr, ...)` and
`CreateShaderResourceView(texture, nullptr, ...)`, so both default views
inherit the swapchain texture's UNORM Texture2D identity.

The selected specialization index is global RVA `0x04E38A08`, written from
player view `+0x3A4` at `0x26C779`. The selected record is therefore
`*(group+0x60) + index*0x88`; the stock cache for its current RTV is RVA
`0x00CA02E0`. When they execute, both conditional late native-CHUD phases bind
group 1/display through `0x274524` before the final CHUD draw. The accepted
external observer saw only player slot zero but did not sample `+0x3A4`, so
normal specialization index zero remains a required runtime check.

Swapchain creation `0x250C4C` proves one buffer, DISCARD swap effect, UNORM
format, sample count one, and shader-input plus render-target-output usage.
Stock wrapper `0x25113C` reads that same swapchain at `0x251195` and calls
Present at `0x2511AA`.

Cleanup `0x2670FC`-`0x26724F` releases and nulls the group resources and views.
Any retained engine RTV/view must follow that view-generation lifetime.
Whole-rasterizer dispose/init, ResizeBuffers, and a separate reset/recovery path
can each recreate the views, so ResizeBuffers alone is not a sufficient
generation boundary for a retained engine RTV. A cold-retained swapchain
buffer has the narrower resource lifetime and must be released before
ResizeBuffers, swapchain replacement, title teardown, or module unload.

This is a direct-to-display path, not Halo 3's internal scene-color path. The
shared Halo 3 learner requires a full-resolution slot-0
`R8G8B8A8_TYPELESS` resource with RT, SRV, and UAV bind flags; applying that
identity rule to Reach would be unsupported and could omit late native CHUD.

The safe first readiness slice retains buffer 0 from the live Present receiver,
then validates it on the cold title worker only when the captured D3D11 creation
flags permit cross-thread device use. Record-0/selected-view values are tracked
only as structural identities and are never dereferenced across the separate
cleanup/recovery paths. The worker creates matching eye caches there; a future
candidate can then let each eye render normally into Reach's display buffer.
Immediately after each original
`player_view_render` returns, the candidate would use same-context
`CopyResource` to snapshot the intended completed world, first-person, and
executed native-CHUD phases into that eye cache. The second eye would remain in
the stock display buffer for desktop continuation and Present. Single-sample
identical descriptors statically select copy rather than resolve; live content,
ordering, copy success, and no-added-frame-latency still require runtime proof.

Direct final-RTV redirection is not the first candidate: direct clears/copies
or SRV feedback could bypass it, and it would leave the Presented stock buffer
unwritten without a copy-back. Production still has to prove the cold resource
identity, live specialization index zero, pointer continuity, same-context copy
success, and exact generation lifetime before admitting the display copy.

### Remaining runtime gate

The static owner, stride, two caller semantics, within-call freshness,
workspace lifetime, final display owner, late-CHUD order, pre-Present capture
placement, and transaction-scoped clear are closed. Before a Reach hook is
eligible, the remaining runtime-evidence and implementation gates are:

- production exact-return routing for the normal caller, stock-only routing for
  screenshot and unknown callers, and complete exactly-one loaded-image checks
  including all 25 canonical frustum bytes;
- production enforcement of the now-observed continuous camera freshness and
  one-second safety interval;
- production propagation of the outer normal-owner token into the proven inner
  candidate, plus serial-reuse, temporal, transitive-side-effect, and
  reentrancy guards;
- inside-active-scope safety and OpenXR mapping for the stock-observed
  pre-scope rebuild,
  plus byte-exact rollback of every touched region;
- live confirmation that the production cold record-0/buffer-0 and
  specialization-zero readiness checks pass, continued identity at the exact
  inner scope, and a successful same-context per-eye `CopyResource`;
- pause, cinematic, split-screen, unload/reload, device-loss, and title-module
  transition behavior;
- callback quiescence and complete detour teardown;
- finite-value, range, index, count, and failure-to-stock guards;
- exact-DLL headset validation plus Halo 3 regression.

Accordingly, all Reach `proof_complete` and `hook_eligible` fields remain
false.

## Hard-off render-candidate foundation

The Halo 3 behavior being matched is one synchronous two-pass transaction at
the inner view renderer: configurable eye order, identical stock starting
state, complete engine-owned world/first-person/native-CHUD work once per eye,
immediate eye capture, and restoration after the pair. In Reach, the entire
source-named `player_view_render` remains indivisible because its world,
effects, first-person, and conditional CHUD subpasses interleave.

`HALOMCCVR_EXPERIMENTAL_REACH_RENDER_CANDIDATE=ON` compiles a separate,
hard-off candidate foundation. It pins the canonical 32-byte
`main_render_view`, 69-byte masked `player_view_render`, and 25-byte frustum
entries and implements allocation-free pure gates for:

- exact normal, screenshot, and unknown outer-return routing;
- slot-zero workspace/player-view validation and the pre-push depth limit;
- opaque module-base/generation-bound preflight, freshness, prepared-frame, and
  direct-copy tokens; freshness is also bound to the exact frame serial,
  observation nonce/time, live gate state, and one-time owner consumption;
- a monotonic display-resource revision and nonce. Only the live resource gate
  can mint a direct-copy-readiness token, and ResizeBuffers, view/pointer drift,
  title teardown, or module-generation change invalidates copied tokens;
- monotonic-generation, non-reentrant, single-prepared-serial outer ownership
  with generation-and-serial-keyed finish/abort and live-owner validation in
  final action selection;
- exact inner return edge, active-view, depth, top-workspace, callback,
  render-camera owner, specialization-zero, camera-validity, copy-readiness,
  and teardown checks;
- strict camera freshness that can cross one second only on a new valid
  transaction with every gap below 500 ms;
- Halo 3-compatible configurable pass order, with invalid passes authorizing
  no write, `+0xA30` false on the inserted first pass, and the original byte on
  the final pass; and
- the exact `0x2B0`, `0xC8`, and `0x2D0` rollback envelopes without treating
  the full `0xA40` player view as reversible state, plus a pure cleanup gate
  that tracks both passes as dirty and is the only source of the exact clean
  completion/abort token required to release an owner.

The same build now contains the first production cold proof path. On the 50 ms
title worker, and only while Reach is the sole detected title, it temporarily
pins the loaded module and verifies the exact backing-file SHA-256, AMD64 PE
timestamp and image size, bounded non-overlapping executable sections, exactly
one loaded-image match at each of the three expected RVAs, both complete body
hashes, all six pinned rel32 edges, and every consumed fixed image range. Its
generation-tagged publication is invalidated on title ambiguity or exit.
Signature scanning, hashing, file I/O, D3D resource allocation, and candidate
logging remain outside Present and every engine render callback.

Present first compares its swapchain with Reach's engine-global swapchain using
one raw pointer read, so unrelated overlay/video Presents cannot consume the
throttle. At most every 250 ms on the exact Reach Present boundary, it reads the
fixed display fields twice into lock-free fixed storage and retains only the
exact live Present swapchain, its buffer 0, and its device across worker handoff.
Record-0 RTV/SRV values are structural continuity identities only and are never
dereferenced by the worker. The snapshot also records device creation flags and
device/immediate-context identities; `D3D11_CREATE_DEVICE_SINGLETHREADED` fails
closed before publication or any worker D3D call. It does no file/hash/signature
scan, allocation, lock, or logging. After loaded-image publication, the 50 ms
worker consumes only that retained snapshot. It checks group 1's count/array/
record-0 identities, specialization index zero, and selected RTV cache, verifies
the proven one-buffer DISCARD/UNORM contract and exact single-sample copy shape,
verifies one D3D11 device and
its immediate context, and transactionally creates two distinct matching Reach-
private eye caches without touching Halo 3/ODST resources. Each refresh first
revokes the old capability and consumes a newly retained buffer/device snapshot;
ResizeBuffers excludes the worker and invalidates
the resource revision even if COM later reuses the same addresses. A temporary
Halo 3/Reach ambiguity invalidates readiness while preserving the resident Reach
generation, so sole-Reach re-entry can safely publish a higher revision.

This is implementation readiness, not live proof: the path has not yet been
run in MCC. It installs no Reach MinHook detour, performs no camera or
engine-memory write, issues no Reach `CopyResource`, captures no eye, and
publishes no Reach lifecycle or capability. `TitleHookPlan::None`,
`TitleCapability_None`, and `ReachAdapter_RuntimeHooksPermitted()==false`
remain unchanged. The runtime wrapper supplies false authorization, so even a
complete static, display, owner, and inner-scope proof still selects stock-once.

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
The accepted source `5d34180` used the first 24 bytes of the canonical 25-byte
frustum entry, omitting its terminal `0x50`. Its run therefore proves that
24-byte prefix unique at the expected RVA, not the full documented entry in
loaded memory. Current source checks all 25 bytes but has not been re-run.
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
Two preflights passed; 29,507 accepted exact-slot observer transactions, all
at slot 0, yielded 29,496 valid camera samples and seven stable windows, with no invalid cameras,
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

The next render gate is a fail-open implementation of the exact outer-owner
token, proven inner `player_view_render` candidate, proposed runtime-unvalidated
`stock_last_window && final_eye` wait policy (stock false stays false),
stock-observed pre-scope camera rebuild, complete rollback, and cold-validated
group-1 buffer copy. The final eye's actual post-call wait byte must persist.
It must refuse stereo unless every identity, scope, finite/range,
specialization-zero, and copy precondition holds. Production must also enforce
freshness, reentrancy, pause/cinematic/split-screen behavior, unload/reload and
device-loss handling, callback quiescence, and complete detour teardown. Still
independently derive observer effects, first-person weapon behavior, HUD
anchor, skeleton and weapon-marker facts, brightness, and motion blur before
enabling the corresponding player-visible behavior. Xbox 360 map symbols may
supply names or call-graph hints only; every fact must be re-proven against
HREK and the pinned MCC x64 module.
