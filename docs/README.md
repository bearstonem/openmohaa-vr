# OpenMoHAA VR documentation

OpenMoHAA VR is a standalone **Medal of Honor: Allied Assault** VR port targeting **Meta Quest and Steam Frame**. Quest 3 is the current tested headset. Steam Frame is a development target with no published build yet.

## Start here

- [Project overview and platform status](../README.md)
- [VR downloads](https://github.com/bearstonem/openmohaa-vr/releases)
- [Quest installation and Android builds](markdown/04-coding/03-android.md)
- [Port status and known issues](../VR_PORT_STATUS.md)
- [Report a VR issue](https://github.com/bearstonem/openmohaa-vr/issues)

The APK contains only the engine. Supply your own original game data and install the release's `autoexec.cfg`, including when upgrading. That config disables development autostart and applies the current dynamic-lighting workaround.

## Working on the port

- [Contributing to OpenMoHAA VR](../CONTRIBUTING.md)
- [Source layout](../LAYOUT.md)
- [Quest renderer notes](../VR_RENDERER_HANDOFF.md)
- [gl4es integration](markdown/04-coding/04-gl4es.md)
- [Dynamic-lighting investigation](markdown/04-coding/05-dlights.md)
- [Reusable Quest porting guide](../QUEST_PORTING_GUIDE.md)

The VR runtime currently depends on Android, EGL, and OpenGL ES. Steam Frame needs platform integration and testing; compiling the desktop engine alone does not provide a Steam Frame VR build.

## Inherited engine reference

These documents cover the underlying OpenMoHAA engine and its desktop tools. Their platform-specific release links and compatibility notes describe upstream capabilities, not validation of the headset port.

- [Desktop installation](markdown/01-intro/01-installation.md)
- [Desktop engine builds](markdown/04-coding/01-compiling.md)
- [Game data, expansions, and launch options](markdown/02-running/01-running.md)
- [Game settings](markdown/03-configuration/01-configuration.md)
- [Server configuration](markdown/02-running/02-running-server.md)
- [Scripting reference](markdown/04-coding/02-coding.md)
- [Engine FAQ](markdown/02-running/03-faq.md)

The [upstream OpenMoHAA project](https://github.com/openmoh/openmohaa) maintains the original engine. Use this fork's issue tracker for VR behavior and headset-specific bugs.
