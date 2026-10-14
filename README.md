# OpenMoHAA VR

**A standalone VR port of Medal of Honor: Allied Assault targeting Meta Quest and Steam Frame.**

[![Quest prerelease](https://img.shields.io/github/v/release/bearstonem/openmohaa-vr?include_prereleases&label=Quest%20prerelease)](https://github.com/bearstonem/openmohaa-vr/releases)
[![Downloads](https://img.shields.io/github/downloads/bearstonem/openmohaa-vr/total)](https://github.com/bearstonem/openmohaa-vr/releases)
[![License](https://img.shields.io/badge/license-GPL--2.0--or--later-blue)](COPYING.txt)

OpenMoHAA VR brings the original game into room-scale VR: head tracking, controller-aimed weapons, tracked hands, physical interactions, and menus and HUD adapted for a headset. The game runs on the headset itself. A PC is needed for the documented sideloading workflow, not for streaming gameplay.

This is the VR development fork of [OpenMoHAA](https://github.com/openmoh/openmohaa). You must supply your own original game data; no Medal of Honor game assets are included.

## Platforms and current status

| Target | Status |
| --- | --- |
| **Meta Quest** | Standalone Android/OpenXR prerelease available. Quest 3 is the headset tested so far; other Quest models are unverified. |
| **Steam Frame** | Standalone VR development target. Platform integration and device validation are still pending; there is no Steam Frame release yet. |

The current VR backend uses Android, EGL, and OpenGL ES. The inherited desktop engine build is useful for development, but it does not establish Steam Frame VR support.

**This is a port in progress.** The latest code review passed desktop and Android builds and targeted regression checks. Its visual changes and performance impact have not yet been measured in a headset. Upstream campaign, expansion, mod, and multiplayer compatibility should not be read as a claim that each has been validated in this VR port.

## Download and install on Quest

Get the APK, `autoexec.cfg`, and `SHA256SUMS` from the [VR releases page](https://github.com/bearstonem/openmohaa-vr/releases). The current release is [0.2.0](https://github.com/bearstonem/openmohaa-vr/releases/tag/vr-quest3-0.2.0).

1. Install the APK:

   ```sh
   adb install -r openmohaa-vr-quest3-0.2.0.apk
   adb shell am force-stop org.openmoh.openmohaa
   ```

2. On a fresh installation, launch once from inside the headset to initialize the app's files directory, then close it. Supply the retail game's `main/` data under `/sdcard/Android/data/org.openmoh.openmohaa/files/main/`. See the [Quest build and installation guide](docs/markdown/04-coding/03-android.md) for the data-copy steps.
3. Copy the release configuration:

   ```sh
   adb push autoexec.cfg /sdcard/Android/data/org.openmoh.openmohaa/files/main/autoexec.cfg
   adb shell am force-stop org.openmoh.openmohaa
   ```

4. Launch the game from inside the headset, under Unknown Sources. Release 0.2.0 appears as **OpenMoHAA**; subsequent source builds use **OpenMoHAA VR**. Do not start it with `adb shell am start`.

**When upgrading, update the configuration too.** Reinstalling the APK keeps the old `autoexec.cfg`; back up personal changes before replacing it. The supplied configuration starts at the normal menu, disables the development cheats and test map, and includes the current lighting workaround. Let the app create its own `main/save` and `main/configs` directories.

The APK is debug-signed with optimized Release native libraries. Version 0.2.0 uses the same signing certificate as 0.1.0 for upgrades. Verify downloaded assets with `sha256sum -c SHA256SUMS`.

## VR features

- Stereo OpenXR rendering and 6DoF head tracking.
- Controller-aimed weapons, tracked hands, stick locomotion, and snap turning.
- Body holsters, grip-driven ladder climbing, and physical grenade throws.
- Menus and mission/death screens displayed on a panel in the room.
- An on-demand HUD on the off-hand **X** button: compass, objectives, health, and ammo.
- Controller-based use interaction and a weapon-selection wheel.

These systems are implemented; controller interactions, weapon alignment, and individual missions still need broader device testing.

## Known issues

- Some fonts can render as black boxes because of an unresolved font-atlas texture issue.
- Dynamic lights can darken the scenery. The supplied `autoexec.cfg` keeps `r_worldDlights 0`; the APK's defaults alone do not apply that workaround.
- Per-weapon grip alignment and remaining foliage behavior need further work.
- Fresh headset performance measurements are pending. Earlier Quest 3 measurements are historical, not benchmarks of the latest release.
- Steam Frame integration and validation remain to be done.

See [port status](VR_PORT_STATUS.md) for implementation details and the historical Quest measurements.

## Development

The active development branch is `vr-quest3`; the project targets both Quest and Steam Frame.

```sh
git clone --branch vr-quest3 https://github.com/bearstonem/openmohaa-vr.git
cd openmohaa-vr
```

- [Quest / Android build and installation](docs/markdown/04-coding/03-android.md)
- [Documentation index](docs/README.md)
- [Source layout](LAYOUT.md)
- [Contributing](CONTRIBUTING.md)
- [Port status](VR_PORT_STATUS.md), [original Quest plan](VR_PORT_PLAN.md), and [renderer notes](VR_RENDERER_HANDOFF.md)
- [Reusable Quest porting guide](QUEST_PORTING_GUIDE.md)
- [Desktop engine build reference](docs/markdown/04-coding/01-compiling.md)

## Feedback and credits

Report port bugs in [this repository's issue tracker](https://github.com/bearstonem/openmohaa-vr/issues). Include your headset, release or commit, map, steps to reproduce, and relevant logs. Steam Frame development reports should include the runtime and build configuration.

Built on the work of the [OpenMoHAA project](https://github.com/openmoh/openmohaa), [ioquake3](https://github.com/ioquake/ioq3), and the F.A.K.K SDK. [Team Beef's RTCWQuest](https://github.com/DrBeef/RTCWQuest) is the reference for much of the VR integration. The original engine documentation, assets, and attribution remain in the tree.

Engine source is licensed under [GPL-2.0-or-later](COPYING.txt). Medal of Honor game data is separately owned and is not distributed here. This is an independent community port, not an official Electronic Arts, Meta, or Valve release.
