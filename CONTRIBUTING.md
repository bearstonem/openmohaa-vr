# Contributing to OpenMoHAA VR

OpenMoHAA VR is an independent VR port of [OpenMoHAA](https://github.com/openmoh/openmohaa), with standalone Meta Quest releases and Steam Frame as a development target. There is no Steam Frame binary yet. These guidelines apply to [bearstonem/openmohaa-vr](https://github.com/bearstonem/openmohaa-vr); upstream maintains its own contribution policies and release process.

Start with the [port status](VR_PORT_STATUS.md), [repository layout](LAYOUT.md), and [renderer handoff](VR_RENDERER_HANDOFF.md). The status document separates device-tested behavior from changes that still need headset validation.

All contributions, including **issues**, **discussions**, **code comments**, **commit messages**, and **documentation**, must be written in *English*. This ensures clarity and consistency for all contributors.

There are many ways to contribute. Contributions are welcome in the following areas:

- Fixing [bugs and crashes](#issues).
- VR rendering, tracking, controller interaction, and HUD improvements.
- Performance improvements backed by measurements.
- Headset testing and reproducible regression reports.
- Documentation and build improvements.
- Steam Frame development and testing, with support claims kept explicit.
- Helping users and proposing enhancements through this repository's issues.

Discuss major features or architecture changes in a [port issue](https://github.com/bearstonem/openmohaa-vr/issues) before investing in a large implementation. Focused fixes, tests, and documentation improvements can be submitted directly.

---

## Issues

Before opening a new issue:

1. Check the [port releases](https://github.com/bearstonem/openmohaa-vr/releases) and [known issues](VR_PORT_STATUS.md). Upstream desktop releases do not include this port's VR changes.
2. Search [existing port issues](https://github.com/bearstonem/openmohaa-vr/issues).

Include the following details to help reproduce and diagnose an issue:

- A clear description and reproducible steps, including the mission/map and game edition.
- Headset model, headset OS/firmware, controllers, and OpenXR runtime/version when known.
- Port release and asset name, or the source commit and build configuration. Include the engine version from the startup log or `version` command as well.
- Relevant VR/render settings, mods, and whether the release's external `autoexec.cfg` was installed.
- Startup and failure logs, including `qconsole.log`, Android logcat, or runtime logs as appropriate. Include a backtrace for crashes.
- For visual or performance issues, whether one or both eyes are affected, and any captures or frame-time measurements.

For Quest logs, begin capture before reproducing the issue: `adb logcat -v threadtime -s openmohaa:V AndroidRuntime:E > openmohaa-vr.log`. See the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md) for the full checklist.

Report port problems here first. If a problem also reproduces in upstream OpenMoHAA, provide that evidence and follow upstream's policies when reporting or contributing there.

## Tools and contribution responsibility

AI-assisted contributions are welcome in this fork. Contributors remain responsible for reviewing, understanding, and validating every submitted change, including generated code and documentation. Explain the problem, the resulting behavior, and the checks performed; distinguish source review, automated tests, and actual headset testing.

Keep changes focused and reviewable. Preserve license headers, copyright notices, and authorship attribution, and identify the source and license of imported code or assets. Do not submit proprietary game data. Tool use does not change these responsibilities.

This policy applies only to OpenMoHAA VR. It does not override upstream OpenMoHAA's independently maintained policies or authorize sending AI-assisted work upstream.

## Coding

### Compatibility

Preserve the engine's compatibility with the original game while adapting presentation and input for VR:

- Assets must be loaded correctly.
- Changes related to networking must be compatible with existing MOHAA clients and servers, these changes must not cause an error/disconnect.
- Scripts/mods must remain functional.
- Preserve single-player campaign behavior; report which missions and expansions were actually tested.
- Keep desktop and dedicated-server builds working when changing shared code.
- Do not change existing package IDs, config/save paths, network identifiers, or binary/module names solely for display branding.

### Coding style & Guidelines

- Match the existing code style in any files you modify.
- Avoid personal tags or author comments (e.g., `// @Name: note`). Git already tracks authorship.
- Build the affected targets and run relevant checks. State when a target or headset was unavailable.
- Follow the surrounding naming conventions, including engine subsystem prefixes such as `CG_`, `CL_`, `R_`, and `VR_`.
- Format changed code using the repository's `clang-format` configuration; avoid unrelated whole-file formatting.
    - In VSCode from the command palette: **Format document**

### Event declaration

When declaring a new Event, use the following structure:

```cpp
Event EV_YourEventName  // Pascal Case naming convention
(
    "name",             // Each parameter on a new line
    flags,
    "format specifiers...",
    "argument names...",
    "description"
);
```

### Code annotations

When modifying existing code from the original game or adding new classes, annotate your changes using one of the following patterns:

#### Additions

```cpp
// Added in OPM
//  Description
```

#### Changes

```cpp
// Changed in OPM
//  Description
```

#### Fixes

```cpp
// Fixed in OPM
//  Description
```

#### Removal

```cpp
// Removed in OPM
//  Description
```

If referencing a specific game version, replace OPM with that version number. Example:

```cpp
// Added in 2.11
//  Make sure to clean turret stuff up
//  when the player is deleted
```
Known versions are `1.00`, `1.10`, `1.11`, `2.0`, `2.1`, `2.11`, `2.15`, `2.30`, `2.40`.

You can also group multiple related functions like this:

```cpp
// Added in OPM
//====
void Function1();
void Function2();
void Function3();
//====
```

For new source files, include the annotation after the license notice.

### Source files

- Always put the license notice as the header of the file (template [here](docs/markdown/05-contributing/01-license-header.md)).
- Only `#include` what's necessary.
- Each source file should contain related classes and functions.

---

## Pull Requests

Pull Requests are the preferred way of submitting code changes.

### Before you start

1. **Discuss major changes first** in this port's issue tracker.
2. **Create a branch** based on this repository's current default branch, rather than upstream's `main`.
3. **Keep PRs as small as possible and focused**: One clear purpose per PR.

### Submitting a PR

- Target this repository's current default branch, unless a maintainer requests a different integration branch.
- Add a short and clear description of what you changed and why.
- Reference matching issues (e.g. `Fixes #123`).
- Make sure your code builds and passes tests.
- Test locally to ensure your changes don't break functionality or compatibility.
- For VR changes, list the headset/runtime, build, and scenarios tested. A successful build alone does not establish device support or a performance improvement.

### Reviews

- Maintainers may ask for tweaks or commit changes directly to your PR, that's normal.
- Stay polite and technical.
- Maintainers decide whether and when a change is ready to merge.

### Additional Notes

- Contributions follow the same license as the project.
- Keep things consistent with existing code and docs.
- Use the [OpenMoHAA VR issue tracker](https://github.com/bearstonem/openmohaa-vr/issues) for port questions and proposals.
