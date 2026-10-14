---
name: OpenMoHAA VR bug report
about: Report a VR, gameplay, rendering, or performance issue in this port
title: ''
labels: bug
assignees: ''

---

Report port issues at https://github.com/bearstonem/openmohaa-vr/issues.
Meta Quest is the current standalone release target. Steam Frame is a development
target; no Steam Frame binary is available yet. Please identify the build you tested.

**Describe the bug**

What went wrong? For visual issues, describe whether it affects one eye or both,
the world, HUD, menus, or controller models.

**To Reproduce**

Steps to reproduce the behavior:

1. Start the game using '...'
2. Load mission/map or multiplayer mode '...'
3. Use these controls or settings: '...'
4. Observe '...'

Does it happen every time? Did a previous port release work?

**Expected behavior**

A clear and concise description of what you expected to happen.

**Headset and build**

- Headset model:
- Headset OS / firmware version:
- OpenXR runtime and version, if known:
- Controllers and input mode:
- OpenMoHAA VR release / downloaded asset name:
- Git commit and build configuration, if built from source:
- Engine version from the startup log or `version` console command:
- Host OS and GPU, if applicable to a development build:

**Game and settings**

- Game edition: Allied Assault / Spearhead / Breakthrough:
- Mission/map and single-player or multiplayer:
- Mods or custom assets:
- Changed VR/render settings and whether the release's `autoexec.cfg` was installed:
- For performance issues: refresh rate, render scale, and measured frame time/FPS, if available:

**Screenshots or recording (optional)**

Attach a short capture when useful. Describe headset-only symptoms that a recording
does not show, such as double vision, flickering in one eye, or tracking drift.

**Logs**

- Attach the startup log and the output around the failure as a text file.
- On Quest, start log capture before reproducing: `adb logcat -v threadtime -s openmohaa:V AndroidRuntime:E > openmohaa-vr.log`.
- Include `qconsole.log` when available (`logfile 2` enables it), plus crash output or a backtrace for crashes.
- For source builds on other targets, include relevant OpenXR/runtime logs and the exact launch command.
- Remove personal information or server passwords before attaching logs.

**Crash dump (optional)**

If available, attach a crash dump or Android tombstone. For a source build, identify
the matching commit and retain its binaries/debug symbols for follow-up debugging.

**Additional context (optional)**

Add any other context about the problem here.
