# Halo MCC VR agent contract

Before changing code, read `CLAUDE.md` and `docs/CURRENT-STATE.md` completely.
`docs/CURRENT-STATE.md` is the authoritative accepted-build pointer. Detailed
reverse-engineering facts live in the evidence documents under `docs/`.

## Baseline discipline

- This is one cumulative multi-title mod. Halo 3 and ODST are not separate
  development lines.
- The public `MCC_VR_ALPHA_0.2.2` release is the current known-good baseline.
  Its runtime source is commit `3a2a11bfc66b36e70f60282e91c9d5436f2e18d1`.
- Begin each candidate from the newest headset-accepted source recorded in
  `docs/CURRENT-STATE.md`. Do not select an old branch, build directory, backup,
  DLL, or ZIP merely because it exists.
- Give every candidate a unique commit and artifact hash. Untested or failed
  candidates do not advance the accepted pointer.
- Revert a failed behavioral experiment. Do not leave it dormant behind a
  switch or stack it into the next candidate.
- Never bulk-remove or consolidate accepted dormant diagnostic/fallback paths.
  Cleanup commit `42a1276` built and launched, then fatally failed at the first
  level transition. Isolate one understood path per candidate and headset test.
- Every successful `tools/package-candidate.ps1` run automatically deploys that
  exact manifest-verified candidate into the dedicated `Halo_MCC_VR` directory;
  do not ask the user for a separate install confirmation. Require MCC and the
  launcher to be closed, preserve the prior installed files, stage and verify
  the candidate hashes, then verify the installed hashes separately. Packaging
  or deployment never authorizes launching MCC or changing `halomccvr.cfg`.

## Halo 3 parity foundation

Halo 3's headset-confirmed player experience is the reference for every other
title: controls, camera ownership, stereo presentation, transitions, HUD,
weapons, comfort, configuration, and lifecycle recovery.

- **Strict implementation-parity rule:** when Halo 3/ODST already implement a
  player-visible feature through a proven engine transaction, every later title
  must reuse that same transaction architecture. For first-person hands and
  weapons, this specifically means one bounded interpolation context per engine
  source/palette transaction, source-pointer matching at every final visible
  palette, and reconstruction into private scratch before the stock palette
  builder. It also requires bypassing the title's native flat-screen
  support-hand weapon IK through a statically verified existing no-weapon-IK
  path, so that native IK cannot overwrite the controller-owned wrist after the
  palette solve. A title adapter may supply only verified title-specific call
  sites, layouts, counts, mappings, and no-weapon-IK control edge. Do not
  substitute live-graph parenting,
  body-only admission, inferred ownership masks, probes-as-runtime-behavior, or
  fallback/approximation paths. If exact transaction parity is not yet proven,
  do not install or arm the affected VR ownership path; continue static evidence
  work while the unclaimed engine transaction remains untouched. Once a VR
  transaction is claimed, any mandatory-path failure rejects that transaction
  and enters teardown; it must never rerender through a flat or stock path.
- `tools/check-reach-fp-parity.ps1` is a mandatory candidate-packaging gate.
  Do not bypass, weaken, or remove it to make a candidate package.
- Reuse shared behavior. Put only verified engine-specific signatures,
  layouts, skeleton facts, and calibration in a title adapter.
- Never copy a Halo 3 offset, structure member, bone, marker, tag meaning, or
  engine constant into another title without title-specific evidence.
- State the exact Halo 3 behavior being matched before implementing a title
  feature.
- Document any unavoidable player-visible difference and obtain explicit user
  approval. An untested approximation is not parity.
- A shared-code or lifecycle change requires a target-title headset result and
  a Halo 3 regression result.

## Render-pipeline parity

The accepted lifecycle arms at the first eligible fresh camera boundary after
the one-second safety interval. Each eye's world, first-person weapon, native
CHUD, and capture work then occur as one transaction. A title adapter may locate
equivalent engine stages with title-specific evidence, but must not add latency,
replace native CHUD with a panel/copy path, or reorder the transaction without
explicit approval.

## Reach evidence sources

- New Halo: Reach feature evidence must come from the official HREK/mod tools.
  Do not derive a new hook, layout, marker, class, constant, or behavior from
  Reclaimer or an archived retail/console binary analysis.
- Existing cumulative accepted behavior is preserved unless a scoped candidate
  replaces it. The loaded MCC Reach module may be used only as the runtime
  match target for an HREK-derived unique signature, executable boundary, ABI,
  and layout proof; it is not a source for inventing a new binding.
- If the HREK identity does not match exactly, reject the complete affected
  transaction. Do not install a mixed partial feature or substitute a copied
  cross-title offset, widget-name heuristic, procedural path, or approximation.

## Safety

- Use unique AOB signatures. Zero or multiple matches must prevent hook
  installation and VR ownership before anything is claimed. This hook-safety
  rule never authorizes a claimed transaction to rerender through stock output.
- Never hook `halo3+0x120DF8`.
- Never patch game files on disk or interact with Easy Anti-Cheat.
- Keep logging, file I/O, locks, COM, allocation, and signature scanning out of
  render and palette hot hooks.
- Preserve finite-value, bounds, index, count, and teardown guards.
- Headset observation outranks desktop appearance and theories. Verify the
  installed DLL's SHA-256 separately and match the source/configuration in the
  first log line. Do not call a runtime fix complete until the user tests that
  exact hash in the headset.
- `camscan` is opt-in and has process-memory write modes. Never build or run a
  write mode without explicit user approval for that offline diagnostic.
