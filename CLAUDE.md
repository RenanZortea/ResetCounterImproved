# ResetCounterImproved — project context for Claude Code

## What this is
A **BakkesMod plugin for Rocket League** that counts *flip resets* (wheels-on-ball
contacts while airborne) during freeplay / custom training, and displays a small HUD
with the reset count, the height of the last reset, and the time between resets.

It is a **clean reimplementation** of the community plugin "Reset Counter Plugin 1.1"
(distributed compiled-only via bakkesplugins.com/plugin/178). The original author's
source is not public. This version was reconstructed from the original DLL's verified
strings/imports (cvar names + hooked game events read out of the binary with Ghidra),
then extended. **The cvar names and the conceptual behavior are faithful to 1.1; the
internal logic is our own.**

Primary motivation for the rewrite: the original only exposed cosmetic cvars and had
**no way to manually reset the counter** (it only auto-zeroed on the game's StartRound).
We added a bindable console command for that. We also fixed the detection logic.

## Core function
The counter is driven by the game's own signal, **not** by guessing from jump/dodge events.

- Hook `Function TAGame.Car_TA.SetVehicleInput` — fires every physics tick while a car
  exists. Used as a per-tick poll.
- Each tick (only while `IsInFreeplay() || IsInCustomTraining()` and local car not null):
  - If `car.AnyWheelTouchingGround()` → **landed → zero the chain** (resetCount,
    lastHeight, timeBetween → 0; drop the baseline). This matches the original
    "chained resets" behavior: each airborne sequence is counted fresh.
  - Else (airborne): read `t = car.GetLastWheelsHitBallTime()` (a game-clock timestamp
    the engine maintains for the last wheels-on-ball contact).
    - First airborne tick of a chain: store `t` as baseline, don't count.
    - If `t` increased vs. the stored value → a new wheels-on-ball reset happened:
      `resetCount++`, `lastHeight = car.GetLocation().Z`, `timeBetween = t - lastHitTime`,
      then `lastHitTime = t`.
- `Function GameEvent_Soccar_TA.Active.StartRound` and `Function TAGame.Ball_TA.Explode`
  → full zero (shot reset / new round).
- Manual console command **`resetcounter_resetcounter`** → full zero. Bindable, e.g.
  `bind R resetcounter_resetcounter`.

### Why this approach
"Wheels touched the ball in the air" has no single clean hookable event. The SDK exposes
`CarWrapper::GetLastWheelsHitBallTime()` which the engine updates on wheel-ball contact,
so polling its change is the most reliable detection and **ignores jumps** (the original
bug: an earlier draft hooked Jump/Dodge BeginState and miscounted jumps as resets).

## HUD / features
Three lines drawn via a registered Drawable: `Resets:`, `Height:`, `Time:`.
Configurable through cvars (subset of the original 1.1 set):
- `resetcounter_enabled` (0/1)
- `resetcounter_posx`, `resetcounter_posy` — text position
- `resetcounter_textsize`
- `resetcounter_dropshadows` (0/1)
- `resetcounter_heightdecimal` (0/1) — decimals on the height number
- `resetcounter_color` — hex color of all text
- **`resetcounter_resetcounter`** — NEW notifier (command), the manual reset.

NOTE: the original 1.1 had more per-element color cvars (resetcolor, heightcolor,
timecolor, rgb_enabled, rgb_speed, samecolorenabled, etc.). This reimplementation
currently uses a single `resetcounter_color`. Re-adding the per-element colors is a
known TODO if full visual parity is wanted.

## Files
- `ResetCounterImproved/ResetCounterImproved.{h,cpp}` — the plugin (our logic).
- `ResetCounterImproved/pch.{h,cpp}` — precompiled header; **defines the global
  `_globalCvarManager`** (declared `extern` in pch.h, defined once in pch.cpp). The
  template's GuiBase/logging helpers depend on this global existing.
- `ResetCounterImproved/GuiBase.{h,cpp}`, `IMGUI/*` — from the Martinii89 BakkesMod
  plugin template; do not hand-edit unless necessary.
- `ResetCounterImproved/ResetCounterImproved.vcxproj` — VS project. `.bak` is the pre-edit
  backup.
- Build output DLL: `ResetCounterImproved/plugins/ResetCounterImproved.dll`. The post-build
  step also copies it to `%APPDATA%\bakkesmod\bakkesmod\plugins\`.

## Build (IMPORTANT — environment quirks)
This machine has **Visual Studio 2026 (v18)**, NOT VS 2022. That caused several issues:

1. **Platform toolset must be `v145`.** The template shipped targeting `v143`
   (VS 2022), which is NOT installed here. The only installed toolset is exposed as
   `v145` (under `...\MSBuild\Microsoft\VC\v180\Platforms\x64\PlatformToolsets\v145`).
   The `.vcxproj` has been retargeted: both `<PlatformToolset>` lines say `v145`.
2. **`_CRT_SECURE_NO_WARNINGS` is required.** The newer toolset treats the template's
   ImGui `strcpy`/`strncpy` deprecation warnings (C4996) as errors. The macro is added
   to PreprocessorDefinitions in both Debug and Release configs.
3. **`_globalCvarManager` undeclared** was fixed by restoring the global into pch.h/pch.cpp
   (see Files). My earlier stripped pch.h omitted it.

### Build command (works headless, outside the IDE)
MSBuild alone fails to resolve the VC toolset paths; run it through the dev environment:

```
cmd /c ""D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat" && msbuild ^
  "%USERPROFILE%\Desktop\ResetCounterPlugin\ResetCounterImproved\ResetCounterImproved.vcxproj" ^
  /p:Configuration=Release /p:Platform=x64 /m"
```

(In the IDE: open `ResetCounterImproved.slnx` / the `.vcxproj`, Release | x64, Build.
The Build menu is hidden if VS opens the folder in "Open Folder" mode instead of the
project — open the `.vcxproj`/solution, not the folder.)

## Deploy / reload (file-lock caveat)
BakkesMod holds the loaded DLL open, so the post-build copy fails with
`os error 32 (file in use)` while the plugin is loaded — **this is not a build failure;
the DLL still builds.** To update a running game:

1. BakkesMod console (F6): `plugin unload resetcounterimproved`
2. Copy `ResetCounterImproved\plugins\ResetCounterImproved.dll` →
   `%APPDATA%\bakkesmod\bakkesmod\plugins\ResetCounterImproved.dll`
3. `plugin load resetcounterimproved`

Because this build is now named `ResetCounterImproved.dll` (class `ResetCounterImproved`,
display name "Reset Counter Improved"), it no longer collides with Blaku's official
`ResetCounterPlugin.dll` — both can be installed side by side.

## Caveats / known issues
- **ABI:** built with v145 while the SDK `pluginsdk.lib` targets v143. It links and loads
  cleanly (API Version 95, "loaded successfully" in bakkesmod.log), but this is a
  cross-toolset link. If a future SDK update causes load crashes, install the
  "MSVC v143 - VS 2022 C++ x64/x86 build tools" component and retarget to v143.
- **Crash history:** an earlier draft access-violated (0xc0000005) because `render()`
  / handlers touched wrappers without null guards and counted jumps. Current code
  null-checks every `getCvar()` and the local car, and only runs in freeplay/training.
  Keep these guards — removing them reintroduces the crash.
- **Detection edge case:** we infer a reset from `GetLastWheelsHitBallTime()` increasing.
  If the engine bumps that timestamp on ball touches that the user does not consider a
  "reset," counts may be slightly high. Tighten by corroborating with `HasFlip()` or a
  stricter airborne gate if reported.
- `SetVehicleInput` polling runs every physics tick; keep the handler cheap (no logging,
  no allocations) to avoid HUD/perf impact.

## Useful SDK references (verified present in this SDK, API v95)
`CarWrapper`: `GetLastWheelsHitBallTime()`, `AnyWheelTouchingGround()`, `HasFlip()`,
`GetFlipComponent()`, `GetLocation()` (returns `Vector{float X,Y,Z}`).
`GameWrapper`: `IsInFreeplay()`, `IsInCustomTraining()`, `GetLocalCar()`, `SetTimeout()`.
