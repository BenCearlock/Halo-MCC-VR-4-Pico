# MCC VR Alpha 0.3.0

**Halo: Reach is now playable in VR**, alongside Halo 3 and Halo 3: ODST.

Reach gets the same treatment as the other two titles: per-eye stereo and 6DOF
head tracking, motion-controller aim, hands and weapon, HUD, cutscenes and
vibration. It is the newest of the three, so expect it to be the roughest.

One control difference to know about:

- **On Reach the left trigger and X are swapped**, so grenades sit on X.

## Halo 3 and ODST fixes

**ODST**

- VR recovers after you open a menu, instead of staying dead until you quit the
  title.
- VR engages reliably at the start of a level, instead of needing a lucky gap in
  the camera.
- Left stick click opens the map screen while the D-pad gesture is held.

**Both titles**

- The desktop window now fits your monitor while the headset keeps rendering at
  full resolution, and the fitted menus stay clickable.
- Your controller is retained when you switch between Halo games in one session.
- The resolution slider now reaches 8K-class, with rescaled F1 tiers.
- Image quality: mod-owned resolve, sharpening, and SMAA 1x / FXAA.

## Install

1. Download `MCC_VR_ALPHA_0.3.0.zip` below and unzip it.
2. Copy `halo3xr.dll`, `halo3xr_launcher.exe` and `halomccvr.cfg` into a folder
   named exactly `Halo_MCC_VR` inside your MCC install
   (Steam > Manage > Browse local files).
3. Make SteamVR the default OpenXR runtime, start Steam + SteamVR, then run
   `halo3xr_launcher.exe`.

### Updating from 0.2.2 — replace your config

**Replace `halomccvr.cfg` with the one in the ZIP. Do not keep your old one.**
This is the opposite of what previous updates told you.

0.3.0 adds settings that older config files do not contain, and there is no
migration step: any setting your old file is missing silently falls back to a
built-in default rather than the shipped value. The most visible casualty is
`fit_desktop_window`, whose built-in default is off while the shipped config
turns it on — so keeping an old config can cap your headset frame rate.
Sharpening, HUD and weapon-alignment values regress the same way.

The shipped config is a tuned, tested configuration rather than bare defaults.
If you want your own tuning back, copy your old `halomccvr.cfg` somewhere safe
first, install the new one, then re-apply your preferences through the F1 menu.

## Known issues

- On Reach, character tags and navpoints are misplaced in 3D, and the
  `hud_curvature` and `hud_vertical_offset` settings have no effect.
- ODST's first captioned opening cutscene can be black. Skip that first scene
  once; do not repeatedly skip or you will miss the working drop sequence.
- MCC can retain multiple title modules after switching games. If a level
  returns to the menu, fully close and restart MCC.
- ODST brightness stays at the game default.

## Verify your download (optional)

```text
ZIP      __ZIP_SHA256__
DLL      __DLL_SHA256__
Launcher __LAUNCHER_SHA256__
```

Windows or antivirus may warn about the unsigned files; that is expected for VR
mods. Allow the two binaries (or the `Halo_MCC_VR` folder) rather than disabling
security. Launch only through the included launcher, with anti-cheat disabled.

Source commit: `__SOURCE_COMMIT__`. Built Release x64 with Halo 3, ODST and
Reach enabled.
