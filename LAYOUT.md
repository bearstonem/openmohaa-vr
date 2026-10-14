# OpenMoHAA VR repository layout

This fork contains the OpenMoHAA engine and its VR port. Meta Quest is the current standalone release target; Steam Frame is a development target with no binary available yet. Existing engine names, package IDs, and data paths remain in place for compatibility.

Start with [VR_PORT_STATUS.md](VR_PORT_STATUS.md) for current support and known issues. The older plan and renderer handoff record design decisions and historical measurements; they are not release support claims.

| Path | Description |
| --- | --- |
| [VR_PORT_STATUS.md](VR_PORT_STATUS.md) | Port status, tested behavior, known issues, and build/deployment notes. |
| [VR_PORT_PLAN.md](VR_PORT_PLAN.md) | Original Quest port survey and implementation plan. |
| [VR_RENDERER_HANDOFF.md](VR_RENDERER_HANDOFF.md) | Renderer migration and gl4es integration history. |
| [QUEST_PORTING_GUIDE.md](QUEST_PORTING_GUIDE.md) | Lessons and reference workflows from the Quest port. |
| `code/vr/` | OpenXR session, frame submission, tracking, and controller input; `vr_common.h` exposes the engine-facing API. |
| `code/cgame/` | Client game presentation, view models, and the grouped VR HUD in `cg_vrhud.cpp`. |
| `code/client/` | Client networking, snapshots, UI integration, and VR frame routing. |
| `code/fgame/` | Server game logic, including gameplay interactions used by the VR port. |
| `code/renderergl1/` | Fixed-function renderer and VR view composition; the default Android renderer uses gl4es. |
| `code/renderergl2/` | Shader-based renderer and its VR integration. |
| `code/renderercommon/` | Shared renderer interfaces, image handling, and related utilities. |
| `code/qcommon/` | Shared engine code, math, filesystem, and configuration handling. |
| `code/corepp/` | Shared C++ infrastructure. |
| `code/server/` | Server networking, snapshots, and client handling. |
| `code/null/` | Platform stubs used by dedicated-server builds. |
| `code/sdl/` | SDL input and platform integration. |
| `code/sys/` | Platform system code, including Android support under `code/sys/new/`. |
| `code/uilib/` | Widget and UI library. |
| `code/script/`, `code/parser/` | Script engine and Flex/Bison parser. |
| `code/skeletor/`, `code/tiki/` | Skeletal models, animation, and the TIKI model system. |
| `code/botlib/`, `code/gamespy/` | Bot support and inherited GameSpy SDK integration. |
| `code/thirdparty/` | Bundled third-party dependencies and their notices. |
| `misc/android/` | Gradle project, Android manifest/resources, and the external `autoexec.cfg` supplied with Quest releases. |
| `misc/linux/`, `misc/macos/`, `misc/windows/` | Desktop packaging and platform resources. |
| `cmake/`, `CMakeLists.txt` | Native build configuration and dependency integration. |
| `.github/` | Build/release workflows and issue templates for this repository. |
| `docs/` | Engine documentation and Android/VR integration notes. |

Use [CONTRIBUTING.md](CONTRIBUTING.md) for development conventions and report port issues in [bearstonem/openmohaa-vr](https://github.com/bearstonem/openmohaa-vr/issues).
