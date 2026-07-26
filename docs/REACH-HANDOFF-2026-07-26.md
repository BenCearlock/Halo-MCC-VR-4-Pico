# Reach handoff - 2026-07-26

Written for whoever picks this up next. Everything below is either **verified**
in this session or explicitly marked as unproven. Prior Reach documentation in
this repo contained at least one wrong claim that cost a day (see "Corrections"),
so treat undated claims elsewhere with suspicion.

## Current build state

- Branch `reach/frame-skip`.
- **Installed and headset-confirmed working:** commit `32377c3`, DLL
  `79EB9C431A0303EF427A84AF4CA794C17A10ED6E1914A458C2EAD031DFAC084F`,
  package `out/candidates/32377c3-reach-fp-parity-20260726-212717879Z`.
- The branch tip additionally contains `a5e030f` and its revert `77e005c`. The
  installed DLL does **not** contain `a5e030f`.
- Rollback: `install-candidate.ps1` requires git `HEAD` to equal the package's
  `source_commit`, so `git checkout --detach <commit>` first.

Confirmed working in the headset on that build: Reach VR crosshair on the
controller aim ray, bullets tracking it, left hand on the controller, floating
hands, stereo/6DOF stable with no unhooking.

## Method that works (and the one that does not)

**HREK first. Retail is for matching only.** `haloreach.dll` is stripped and
optimized; reading it to *discover* behavior yields plausible wrong answers.
HREK (`N:/SteamLibrary/steamapps/common/HREK/`) has symbols, source paths,
assert text and tag definitions. `reach_tag_test.exe` is the debug build and is
the most informative.

**The retail resolution chain that works** (proved twice):

1. find the script-function name string - must occur exactly once
2. find the single qword pointing at it - that is its script-table entry
3. **entry + 0x18** is the implementation

Byte-matching HREK prologues does **not** work: the obvious
`chud_show_crosshair` prologue matches **484 times** in retail because it is a
shared script-binding prologue.

**Do not brute-scan process memory for structures.** Tried twice for the
navpoint array: 593 hits with a loose filter (all unit vectors), 317,855 with a
tighter one. Useless.

## Tools

`tools/reach-probes/` (all READ-ONLY, `PROCESS_VM_READ` only, never write to the
game). Run with `py -3` from that directory.

| script | purpose |
| --- | --- |
| `petool.py` | PE load, RVA/file mapping, string + xref helpers |
| `rdis.py` | offline disassembler + RUNTIME_FUNCTION bounds (`fn` / `at` / `find`) |
| `callers.py` | direct callers of a function, with argument setup |
| `chud_strings.py` | find strings and their code xrefs in any module |
| `chud_live.py` | attach to MCC, resolve the CHUD TLS chain, dump widget records |
| `chud_full.py` | full CHUD diagnostic incl. what changes while the player acts |
| `chud_watch.py` | watch CHUD flag/alpha arrays for changes |
| `chud_map.py` | map CHUD alpha indices to widget names via `chud_fade_*` |
| `cam2.py` | compare Reach's cameras live (head vs aim divergence) |
| `crosshair_state.py` | find crosshair//state bytes by what toggles |
| `navpoint_find.py` | locate the navpoint array via TLS blocks |
| `find_chud.py` | structural scan for a `chud_draw_widget` homolog |

## Verified facts

### Reach has no Halo 3/ODST class-2 crosshair gate

Halo 3 and ODST each contain exactly one `game_is_playback`-gated class-2
visibility predicate inside `chud_draw_widget`; production NOPs the two-byte
short-circuit and hooks the predicate.

| module | chud_draw_widget | class gate |
| --- | --- | --- |
| `halo3.dll` | `0x2EDF24` | `+0x84` |
| `halo3odst.dll` | `0x329488` | `+0x81` |
| `haloreach.dll` | **none** | **none** |

Structural scan (functions with a `movsx byte [desc+3]` + `byte [desc+4]` pair
*and* a class-2 compare): halo3 1 hit, odst 1 hit - both landing on the shipped
addresses - haloreach **0**. `mov eax,2` + `cmp ax,word[reg+4]` occurs once in
halo3 and **zero** times in haloreach. HREK confirms: Reach's CHUD has no
`chud_widget.cpp` and no `chud_draw_widget` symbol. **No signature can port this
mechanism.** Reach uses the procedural VR reticle instead.

### Reach draws its CHUD TWICE

| path | HREK entry | called from | camera in VR |
| --- | --- | --- | --- |
| `chud_draw_screen_LDR` | `0x8B67C0` | inside `player_view_render` (`0x834490-0x835598`, call at `0x8354AE`) | per-eye VR camera |
| `chud_draw_screen` | `0x8B6320` | UI compositing pass (caller `0x50B780`, at `0x50BD58`) | stock camera |

`player_view_render` calls `0x8B67C0` exactly once and never calls
`chud_draw_screen`. `0x8B6320` is `chud_draw_screen(user_index)` - own profiler
scope, user index gated `!= -1` and `<= 3`. The compositing caller runs
`composite tile -> preui screen_shaders -> user_interface_render ->
chud_draw_screen -> overhead_map -> postui screen_shaders`.

### CHUD widget alpha layout (real, but inert to write)

Three parallel arrays of nine widgets in one record, derived by disassembling
every `chud_fade_*_for_player` implementation in `haloreach.dll`:

```
current alpha  chud_globals + 0x32C + i*4
fade target                 + 0x350 + i*4
fade duration               + 0x374 + i*4

i=1 weapon stats   i=2 CROSSHAIR   i=3 shield    i=4 grenades
i=5 messages       i=6 motion sensor   i=7 chapter title   i=8 cinematics
```
`chud_globals = *(void**)(TLS[tls_index] + 0x5B0)`; `tls_index` is read from
`chud_show_crosshair`'s own `mov ecx,[rip+rel32]` at implementation `+0x30`
(`haloreach.dll+0x1B3190`, resolved via the script-table chain; TLS index global
at `+0xC17B18`).

**Writing these does nothing.** They read `1.000` across 860 live samples while
the mod wrote `0` every frame. The engine drives them elsewhere. Also: an early
attempt wrote `+0x334` at `0xC60` strides for "16 slots" - only the first is a
crosshair; the rest corrupt unrelated CHUD records and drag player markers with
the weapon. There is no stride; there is one crosshair.

### Navpoint structure

HREK `chud_navpoints.cpp`, builder `reach_tag_test.exe 0x907F4E`:
- array: 20 entries, stride `0x88`, `position_worldspace` at `+0x3C/+0x40/+0x44`
- `navpoint.position_worldspace = object_position (obj+0x38C/0x390/0x394) +
  authored_offset_vec3 * scale`
- HREK wrapper `0x8BA510`: block = `*(TLS[idx] + 0x8C0)`, array at
  `block + 0x604C + user*0x10D68` (HREK offsets; retail differs)

**The stored world positions are correct by construction.** The defect is in
projection, not in how navpoints are built.

### Anchor types

`chud_anchor_type_enum` (HREK `0x1870CC0`) splits exactly along the observed
symptom:
- screen-anchored (`top left`, `messaging`, `motion sensor`, ...) - never use the
  camera. These are correct in VR.
- object-anchored (`<tracking object>`, `<scripted object>`, `<player>`,
  `weapon target`, `hologram target`, ...) - resolved through a basis matrix.
  These are the broken squad-mate arrows.

Halo 3 and ODST both hook `chud_compute_anchor_basis`
(`halo3.dll+0x2EE234`, size `0x8ED`, ABI
`bool(int userIndex, void* drawWidgetData, int anchorType, void* basis)`,
called from widget-draw subroutines `0x2EEB80` and `0x2F0C50`).
**Reach hooks nothing equivalent.** Finding Reach's homolog is the open task.

### Reach title lifecycle (this was the crosshair blocker)

Halo 3 has `PublishHalo3Lifecycle`, ODST has `PublishOdstLifecycle`, Reach had
**neither**, so its runtime slot reported `armed=false` with zero capabilities
forever. `TitleRuntimeMaskUnarmedCapabilities` then stripped every arm-gated
capability, `Game_HasTitleCapability(TitleCapability_ControllerAim)` returned
false, and the reticle bailed before `EnsureReticleChain` could run. Symptom:
stereo swapchain created with no crosshair swapchain, plus
`Runtime mode: gameplay -> loading` flapping ~10x/sec during ordinary play.

Fixed by `PublishReachLifecycle` (publish **only on change**; republishing
identical state keeps the shared snapshot "pending", which reports zero
capabilities by design). The reticle also now asks
`Game_OwnsReachAuthoredReticle()` **directly**, exactly as ODST asks
`Game_IsCameraOnlyBringup()` - that directness is why ODST worked first try.

**`ArmIk` is deliberately withheld** from `kReachRuntimeCapabilities`: granting
it attached the left hand to the player's face because Reach's arm IK is not
solved. Only grant it after that is proven in the headset.

## Corrections to existing documentation

`docs/REACH-SIGNATURE-EVIDENCE.md` previously stated that `player_view_render`
reaches both `chud_draw_screen_LDR` and `chud_draw_screen`, implying all CHUD
work happens inside the per-eye transaction. Only the LDR path is inside it.
That incomplete claim caused repeated dead ends because every check inspected
the path that works. Corrected in commit `4741e5d`.

## Disproven - do not retry

- **"Local rebuilds are cursed."** Working DLL `29ADE993` and failing DLL
  `AACB01C9` have **byte-identical `.text`**; only the embedded commit string and
  PE checksum differ (47 bytes). Behavior differences were timing, not the binary.
- **"CHUD uses a second aim-only camera."** The render camera
  (`kReachRenderCameraOwnerRva 0x04E38A90` -> `player_view + 0x3B0`) measured the
  full VR view (179 deg incl. head) live. The 52 deg reading on camera-stack
  workspaces was the probe catching restored state.
- **Stale secondary camera block.** `ReachStereoTransaction` already mirrors
  primary compact+derived into `kReachSecondaryCompactOffset`/`...DerivedOffset`.
- **Writing the CHUD alpha array** to hide the crosshair (see above).
- **HREK assert-validator shapes in retail** - asserts are compiled out.
- Candidates eliminated for `chud_compute_anchor_basis` in HREK: `0x910AB0`
  (a constructor), `0x9654A0`, `0x8E0320`, `0x909140`, `0x88F260` (none writes a
  basis), and `0x92D265`/`0x925D61`/`0x919350` (basis-shaped writers with no
  direct callers - likely unwind chunks, resolve real entries first).

## Open problems

1. **Squad-mate arrows (`<tracking object>`/`<scripted object>` anchors) follow
   the weapon instead of staying on characters.** Same in both eyes, so drawn
   once, outside the per-eye loop. Next step: find Reach's
   `chud_compute_anchor_basis` homolog (big function, ~26-case anchor switch,
   writes a 4x3 basis to its 4th argument) and make it resolve object anchors
   against the VR camera. Unproven idea, recorded as an idea only: our VR camera
   is restored when `player_view_render` returns, so the compositing CHUD sees
   the aim-driven stock camera. A build that deferred that restore
   (`a5e030f`) was **rejected by the user and reverted**; it was never confirmed
   either way.
2. **One muzzle flash texture is misaligned from the weapon.** User-corrected:
   one effect with two textures, one misaligned - not two competing draws. It is
   an effect on a weapon marker, not CHUD; chase it through the first-person
   marker/palette transform.
3. **Reach's flat centre crosshair is still drawn** alongside the VR one. The
   alpha array is inert; the real draw path is unlocated.

## Process notes

- Do not roll back a build the user reports as working, even if a detail is
  wrong - adjust from it and ask first.
- Do not claim something is fixed before the user has tested it.
- One behavioral change per candidate; a clean build, passing tests and a passing
  gate prove nothing about runtime behavior.
- `tools/check-reach-fp-parity.ps1` is a consistency check, not a design
  authority. It was cut from 184 rules to 33 because the deleted rules asserted
  exact implementation text, never caught a defect, and blocked correct changes.
- Every teardown path now logs a reason. Keep that; two bugs were diagnosed from
  a single log line in seconds after it was added.
- Preserved runtime logs from previous installs live in
  `out/deploy-backups/<hash>-before-<hash>/halo3xr.log` - diff against them
  before theorising.
