# Building OpenMoHAA VR for Meta Quest

OpenMoHAA VR runs **Medal of Honor: Allied Assault** in standalone VR through
OpenXR. This guide builds the `arm64-v8a` Android APK for the Meta Quest family;
Quest 3 is the tested headset. Other Quest models need validation. Steam Frame
is a planned target; the current VR runtime implementation is Android-only.

You need the original retail Allied Assault game data. It is not included in
this repository or the APK. Prebuilt packages are available from
[OpenMoHAA VR releases](https://github.com/bearstonem/openmohaa-vr/releases).

> On Android, the fixed function renderer runs through gl4es, which maintains
> its own GL state. For rendering problems, read
> [the gl4es boundary](04-gl4es.md) and the
> [VR port status](../../../VR_PORT_STATUS.md).

## What you need

| Tool | Version |
|---|---|
| Android NDK | r26 or later |
| Android SDK | platform 34, build-tools 34, platform-tools (`adb`) |
| JDK | 17 |
| Gradle | 8.0 or later |
| CMake | 3.25 or later |
| Ninja, flex, bison | installed on the build host |

Set `ANDROID_NDK_HOME` and `ANDROID_HOME` to your installed NDK and SDK paths.
Enable developer mode and USB debugging on the headset, and authorize the host
when prompted. Run the following commands from the repository root unless a
step says otherwise.

## 1. Build the engine

CMake builds the native libraries; Gradle packages them afterward.

```sh
cmake -B build-gl1 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-29 \
  -DANDROID_STL=c++_shared \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SERVER=OFF \
  -DBUILD_RENDERER_GL1=ON -DBUILD_RENDERER_GL2=OFF -DUSE_GL4ES=ON

cmake --build build-gl1 -j"$(nproc)"
```

`ANDROID_STL=c++_shared` lets the APK's native libraries share one C++ runtime.
Android enables `USE_OPENXR` automatically. The default renderer is
`renderergl1` through gl4es; the alternative GLES renderer can be selected with
`-DBUILD_RENDERER_GL1=OFF -DBUILD_RENDERER_GL2=ON -DUSE_GL4ES=OFF`.

SDL2, OpenAL Soft, gl4es, and the Khronos OpenXR loader are downloaded and built
from source. Existing source trees can be supplied with `SDL2_SOURCE_PATH`,
`OPENAL_SOURCE_PATH`, `GL4ES_SOURCE_PATH`, and `OPENXR_SOURCE_PATH` CMake options.
Android links the renderer into the client, disables HTTP downloads and QVM
builds, and stages the APK libraries in `build-gl1/apk-libs/arm64-v8a/`.

## 2. Package the APK

```sh
cd misc/android
gradle assembleDebug -PnativeLibsDir=../../build-gl1/apk-libs
cd ../..
```

The output is `misc/android/app/build/outputs/apk/debug/app-debug.apk`. Keep the
`nativeLibsDir` override: the checked-in Gradle default names a different build
directory. For another build location, supply its absolute `apk-libs` path.

Gradle reads `android-build.properties` beside `apk-libs` to find the SDL Java
classes belonging to the SDL library CMake built. Do not mix those classes with
a different SDL version. After rebuilding, verify that the APK is newer than the
staged native libraries so you install the current code.

## 3. Install the APK and retail data

```sh
adb install -r misc/android/app/build/outputs/apk/debug/app-debug.apk
adb shell am force-stop org.openmoh.openmohaa
```

The Android package ID remains `org.openmoh.openmohaa`, including for upgrades.
Launch the app once from inside the headset to initialize its external files
directory, then close it. Game data belongs in:

```text
/sdcard/Android/data/org.openmoh.openmohaa/files/main/
```

Copy **all** Allied Assault `.pk3` archives from your retail installation's
`main/` directory. Replace the example source path below with your own:

```sh
adb shell mkdir -p /sdcard/Android/data/org.openmoh.openmohaa/files/main
adb push /path/to/retail/main/*.pk3 \
  /sdcard/Android/data/org.openmoh.openmohaa/files/main/
```

Do not pre-create or copy `main/save` or `main/configs` with `adb`. Let the app
create these directories so it can write saves and settings. Directories
created by the shell can leave the app unable to write; changing their mode
does not fix ownership. Keep existing saves when upgrading. A save-directory
permission failure can appear as a black screen on New Game because the first
autosave fails.

## 4. Install the VR configuration and launch

Copy the supplied configuration after installing or upgrading:

```sh
adb push misc/android/autoexec.cfg \
  /sdcard/Android/data/org.openmoh.openmohaa/files/main/autoexec.cfg
adb shell am force-stop org.openmoh.openmohaa
```

`autoexec.cfg` is external game configuration, not an APK asset. Reinstalling
with `adb install -r` preserves the previous file, so replace it explicitly.
Back up any personal edits before replacing it. The supplied configuration
starts at the normal menu with cheats, automatic map loading, and diagnostics
disabled. Keep its `r_worldDlights 0` setting: it works around the unresolved
[dynamic-light darkening defect](05-dlights.md), and the APK alone does not
apply that workaround.

Launch **OpenMoHAA VR** from inside the headset (the published 0.2.0 APK is still
named **OpenMoHAA** in the launcher). Do not use `adb shell am start`;
it has intermittently left Quest in Meta's loading environment. Force-stop
after each APK update because `adb install -r` can leave the old process running.

## Logging and implementation

Capture engine output before reproducing a problem:

```sh
adb logcat -s openmohaa:V > openmohaa-vr.log
```

The engine redirects stdout and stderr to the same tag. Its Android crash
handler reports the fault address and ARM64 `pc`/`lr` registers as well as an
unwind trace. If logcat has no crash report, `adb shell dumpsys activity exit-info`
can still identify the termination signal.

`SDLActivity` loads `libSDL2.so` and `libopenmohaa.so`, then calls the engine's
`SDL_main`. The APK also carries OpenAL, gl4es, the OpenXR loader, and
`libc++_shared.so`; the engine loads `libgame.so` and `libcgame.so` at runtime.
OpenXR uses SDL's EGL context and supplies the eye swapchain images. The
[port status](../../../VR_PORT_STATUS.md) records the architecture, known
limitations, and which changes still need headset validation.
