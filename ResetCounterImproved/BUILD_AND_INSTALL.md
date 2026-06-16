# Reset Counter Improved — Build & Install

You now have your own copy of the plugin source with one added console command:

```
resetcounter_resetcounter
```

…which zeroes the counter on demand and is bindable to any key. This survives plugin
reloads and follows the `resetcounter_*` naming pattern, exactly as requested.

---

## 0. What you have

- `ResetCounterImproved.h` — class declaration
- `ResetCounterImproved.cpp` — all logic (cvars, hooked events, the new notifier, HUD)
- `pch.h` — precompiled header

This is a **clean reimplementation** based on the original 1.1 binary's verified cvars and
hooked game events. The cosmetic cvars and event hook names are exact (read out of the DLL).
The chain-counting logic is reconstructed to match observed behavior — tune it if your taste
for "what counts as a reset" differs (see Section 5).

---

## 1. Install the toolchain (one-time)

1. **Visual Studio 2022** (Community is fine). During install, check the workload
   **"Desktop development with C++"**.
2. **BakkesMod** must already be installed (you have it).
3. Get the **BakkesMod plugin template** — the fastest path:
   - Install the *BakkesMod Plugin Template* from the SDK repo:
     https://github.com/bakkesmodorg/BakkesModSDK
   - Or use the community "BakkesMod Plugin Template" VS project generator.

The template matters because it wires up the include paths to `%APPDATA%\bakkesmod\bakkesmod\bakkesmodsdk`
and sets the correct **x64 / Release** config and the post-build copy step.

---

## 2. Create the project

1. In Visual Studio: **File → New → Project → BakkesMod Plugin Template**.
2. Name it `ResetCounterImproved`.
3. Once it generates, **replace** the template's `.h`, `.cpp`, and `pch.h` with the three
   files in this folder. (Keep the template's `.vcxproj`, `pch.cpp`, and any
   `*GeneratedSettings*` it created.)

> If your template generated a settings file like `ResetCounterImprovedGeneratedSettings.h`,
> leave it; it's harmless. The `RenderSettings()` here is self-contained.

---

## 3. Build

1. Set the configuration dropdowns to **Release** and **x64** (BakkesMod is 64-bit only).
2. **Build → Build Solution** (Ctrl+Shift+B).
3. The template's post-build step copies the DLL to:
   ```
   %APPDATA%\bakkesmod\bakkesmod\plugins\ResetCounterImproved.dll
   ```
   If your template has no post-build copy, copy it there manually.

> This build is named `ResetCounterImproved.dll` (display name "Reset Counter Improved"),
> so it does NOT clash with Blaku's official `ResetCounterPlugin.dll` — you can run both
> side by side.

---

## 4. Load and bind

Open the BakkesMod console (**F6**) and run:

```
plugin unload resetcounterimproved
plugin load resetcounterimproved
```

Test the command directly:

```
resetcounter_resetcounter
```

The on-screen counter should snap to `Resets: 0`.

Bind it to a key (example: the **R** key — pick whatever is free):

```
bind R resetcounter_resetcounter
```

To make the bind permanent, also run `writeconfig`, or add the bind via the
**BakkesMod → Bindings** tab so it's saved across sessions.

---

## 5. Optional: fix the wall-checkpoint problem at the source

The original only resets/counts off `Car_TA.EventLanded` (a physics landing), which is why a
wall-checkpoint teleport never registered. Now that it's YOUR code, you have two clean fixes:

**(a) Manual** — just press your `resetcounter_resetcounter` bind after loading a wall
checkpoint. Simple, already works.

**(b) Automatic** — hook the freeplay-reset path so a checkpoint load also drives the counter.
In `onLoad()`, add a hook on the checkpoint/teleport event your checkpoint plugin fires, or on
`Function TAGame.Car_TA.SetVehicleInput` gated by a "was teleported" flag, and call
`resetCounter()` (or increment) from it. If you use the FreeplayCheckpoint plugin, hook the
function it calls on load and route it to `resetCounter()`.

---

## 6. Notes / gotchas

- **`registerNotifier` vs `registerCvar`:** the original binary only imported `registerCvar`,
  which is exactly why no console command existed to reset it. Your build adds
  `registerNotifier`, so the SDK link now pulls that symbol in — this is the whole point and it
  "just works" when compiled against the SDK.
- **`PERMISSION_ALL`** lets the command run anywhere. If you want it freeplay-only, swap to
  `PERMISSION_FREEPLAY`.
- **SDK signature drift:** if `registerCvar`'s argument list errors on your SDK version,
  reduce it to the 3-arg form `registerCvar(name, default, description)` — the extra
  min/max/range args are optional and only matter for the settings sliders.
- The counter variable is `resetCount` (in the header). Anything that should zero the count
  calls `resetCounter()`.
