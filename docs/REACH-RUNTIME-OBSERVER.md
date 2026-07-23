# Halo: Reach runtime observer

Status: **IMPLEMENTED_UNRUN**. No runtime log has been accepted, no Reach hook
is authorized, and this tool does not advance the accepted-build pointer.

`reach-runtime-observer.exe` is a standalone, external evidence tool for the
pinned Steam `haloreach.dll`. It observes stock MCC from another process. It is
not `halo3xr.dll`, does not load code into MCC, and cannot enable Reach VR.

## Safety contract

The observer requests exactly:

```text
PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
```

It uses process and module snapshots, `VirtualQueryEx`, `ReadProcessMemory`,
and read access to the loaded module's backing file. It installs no hooks or
detours, injects no DLL or thread, changes no page protection, attaches no
debugger, suspends no thread, and writes no game memory or MCC file. Its only
write is its own log, which it creates exclusively; it refuses to overwrite an
existing path. Closing the observer only closes its own handles and log.

Run it only with the stock Steam MCC anti-cheat-disabled launch option. Do not
use `halo3xr_launcher.exe`. The observer refuses a process that has
`halo3xr.dll` loaded, because injected-mod activity would contaminate the stock
evidence. It also refuses an ambiguous process selection; the optional
`--pid` argument is available when more than one MCC process exists.

The observer derives the pinned Steam MCC installation root from the selected
MCC process. It opens the process image, observer executable, installation
root, and output parent for attributes and resolves them with
`GetFinalPathNameByHandleW` to normalized volume-GUID paths. It holds the
resolved output-parent handle without delete sharing and creates the
single-component log name relative to that exact handle with `NtCreateFile`,
`FILE_CREATE`, and `FILE_OPEN_REPARSE_POINT`. DOS aliases, `\\?\` spellings,
symlinks, directory junctions, ancestor renames, or a raced leaf reparse point
therefore cannot redirect it into MCC. Alternate-data-stream output names are
rejected. The running observer executable is also opened once, canonicalized
and hashed from that same handle, with write/delete sharing denied, and held
through the first log line. The observer refuses to run if either its
executable or output is inside the resolved installation. Keep both files in
the diagnostic package outside MCC. The observer does not install or launch
the game.

## Cold loaded-image preflight

Each new Reach module mapping must pass all of these checks before sampling:

- the backing `haloreach.dll` has SHA-256
  `738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894`;
- the loaded PE is AMD64, timestamp `0x68A0EFE1`, with both PE and mapped
  `SizeOfImage` equal to `0x04EDA000`;
- the exact `main_render_view` entry AOB occurs once across executable
  sections, at RVA `0x000C31F4`;
- the 515-byte `main_render_view` body has SHA-256
  `95DF3EFFF9AC6EE29887D1272CCA8D7BF3E58F87041BAD8032107825B733FE89`;
- the exact camera/frustum AOB occurs once across executable sections, at RVA
  `0x00287F58`;
- six proven `rel32` call edges still resolve to setup, both
  `main_render_view` callers, `player_view_render`, the outer main-render
  owner, and the stock Present wrapper;
- the four-slot player-view array, rasterizer-camera workspace, and active-view
  global are committed, readable `MEM_IMAGE` ranges owned by that Reach
  mapping;
- a final module snapshot still identifies the same mapping and still contains
  no `halo3xr.dll`, and Reach is still the sole resident title module.

Zero or multiple AOB matches, a partial read, any identity/range mismatch, a
module reload during preflight, or a loaded mod DLL fails closed for the
observer. It does not attempt a fallback RVA or write anything to recover.
Every unload/reload that the observer sees creates a new observation session
and requires a fresh preflight, even if Windows reuses the same base address.
Module state is polled every 100 ms, so a complete unload/reload that occurs
between polls and returns at the same base with the same mapping identity can
be missed. The log therefore proves only the reloads it records.

## What is sampled

Sampling runs only while Reach is the sole resident supported title module.
If MCC retains Reach alongside another title module, the observer reports an
ambiguous-resident state, pauses sampling, and resets the freshness window.

The observer polls active-view pointer RVA `0x04E389A8`. A new session or
continuity reset must witness a null value before it can count a non-null
transaction, so attaching in the middle of a set/use/clear pulse cannot seed
the freshness ledger. On each later observed non-null transition it accepts
only an exact slot address in the four-object array at RVA `0x029F2B90`, with
Reach-proven stride `0xA40`. A pointer inside an object but not at its start, or
outside that array, is recorded as outside the normal player-view scope.

For an exact normal slot, it reads the complete `0x2A8` rasterizer-camera
workspace at RVA `0x00C9FAE0` between two active-pointer reads. A changed or
cleared second pointer makes that sample torn and unusable. A sample is usable
only when both the primary compact camera at `+0x000` and the secondary compact
camera at `+0x154` independently pass the Reach-specific checks below:

- finite position at `+0x00`, forward at `+0x0C`, and up at `+0x18`;
- forward and up squared lengths each within a strict `0.001` of one, and
  absolute forward/up dot product strictly below `0.001`;
- finite vertical FOV at `+0x28`, strictly between `0.0001` and
  `3.1414928436`;
- ordered signed 16-bit `y0,x0,y1,x1` window and render bounds at `+0x38` and
  `+0x4C`;
- ordered client bounds at `+0x5C`, with zero origin and both extents at least
  eight pixels.

The observer also compares the two validated compact-camera byte ranges.
Equality is counted as evidence but is not required, because sampling can
overlap an engine update.

Freshness is tracked separately for each of the four player slots. An observed
transaction remains fresh for less than 500 ms. A stable window requires one
and only one fresh slot, at least two usable transactions for that slot, no
500 ms gap, and a last-usable-transaction minus first-usable-transaction span
strictly greater than 1000 ms. If two slots are fresh together, every slot
window is reset and the observer records a refused multiple-owner interval;
interleaved split-screen work therefore cannot satisfy the gate. The log calls
this **observed** stability only; it does not arm VR. Sampling can miss the
engine's short set/use/clear pulse, so a missing pulse or freshness reset is
inconclusive rather than proof that Reach did not render.

## First desktop evidence pass

A headset is not needed for this pass.

1. Close MCC and any `halo3xr` launcher. Start stock Steam MCC with anti-cheat
   disabled, and wait at the MCC shell.
2. From the observer package directory, run:

   ```powershell
   .\reach-runtime-observer.exe
   ```

   The console reports the selected process and log path. The first log line
   records the exact observer executable's SHA-256 as well as its source
   identity.
3. Start a Reach Campaign mission. During ordinary gameplay, move and look
   around, use zoom, then pause and resume. Let a cinematic run if one is
   conveniently available.
4. Save and Quit to the MCC shell, then enter Reach again. This exercises the
   title absence/re-entry path. If practical, return to the shell and open a
   different title once to record MCC's multi-title residency behavior.
5. Return to the shell, press `Ctrl+C` in the observer window, and retain the
   complete log.

Useful optional arguments are:

```text
--pid <decimal>       select one MCC process explicitly
--duration <seconds>  stop automatically after that duration
--output <path>       choose the log file
--help                print usage
```

Do not copy a DLL into MCC for this test. If preflight fails, stop and preserve
the log; do not bypass an identity, uniqueness, range, or contamination check.
Stopping after a successful preflight but before any usable camera sample is
intentionally a nonzero, inconclusive result; preserve that log too.

## Reading the result

A useful log contains a loaded-image preflight pass, normal-slot transactions,
valid compact-camera samples, and an observed stable window during gameplay.
It should also show freshness resets without crashing when Reach is absent,
paused long enough, ambiguous with another resident title, or unloaded, then a
new preflight after a reload. Periodic and final summaries retain slot,
validity, torn-snapshot, camera-change, and freshness counts.

That evidence can corroborate the pinned loaded image, sampled workspace
validity, continuous observed freshness, and the load/unload handling that was
actually observed. An exact nonzero slot address can also runtime-corroborate
the static `0xA40` slot stride. Slot 0 alone corroborates only the array base,
not the stride. It cannot prove:

- which static caller produced a sampled pulse, or the unresolved alternate
  caller's semantics;
- that every transaction was observed;
- that every unload/reload was observed, including a complete same-base reload
  between 100 ms module polls;
- an owning GPU render target, capture-target lifetime, or copy success;
- a runtime world/first-person/native-CHUD/capture order beyond the existing
  static control-flow proof;
- device-loss behavior, detour callback quiescence, or detour teardown;
- stereo eye mutation, OpenXR submission, headset parity, or player-visible
  Reach VR behavior.

Those remain separate gates. Until an exact observer artifact is run and its
log reviewed, even the live loaded-image and freshness claims remain unproven.
