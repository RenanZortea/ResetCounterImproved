# Reset Counter Plugin

A [BakkesMod](https://bakkesmod.com/) plugin for Rocket League that counts **flip
resets** (wheels-on-ball contacts while airborne) during freeplay and custom
training, and shows a small, customizable HUD.

This is a clean reimplementation of the community "Reset Counter Plugin 1.1",
rewritten with accurate detection and a number of new features.

## Features

- **Accurate flip-reset detection.** A reset is counted only when a wheel-ball
  contact actually restores your flip (`HasFlip()`), so grazes and partial
  touches don't inflate the count. Re-touching the ball while you still hold the
  reset's flip is ignored — you must spend the flip (dodge) before the next one
  counts.
- **HUD with selectable lines:** reset count, height, time since last reset, and
  average time between resets. Toggle any line on/off.
- **Height as a 0–100 scale.** Ground = 0, ceiling = 100. Show the **ball's**
  height (default), the **car's**, or both.
- **Keep count after reset.** When a run ends, the HUD stays frozen on that run's
  stats for a configurable number of seconds so you can read them.
- **Personal best** tracking (persisted), with a reset button.
- **Custom text overlay** with independent position/size — handy for content
  creators leaving a note on screen for clips.
- **Manual reset command** `resetcounter_resetcounter` (bindable, e.g.
  `bind R resetcounter_resetcounter`).
- Configurable position, text size, color, and outline.

## Console variables

| Cvar | Default | Description |
|------|---------|-------------|
| `resetcounter_enabled` | `1` | Master enable |
| `resetcounter_posx` / `resetcounter_posy` | `5` / `5` | HUD position |
| `resetcounter_textsize` | `2` | Text size |
| `resetcounter_dropshadows` | `1` | Black outline behind text |
| `resetcounter_color` | `#FFFFFF` | Text color |
| `resetcounter_heightdecimal` | `0` | Decimals on the height value |
| `resetcounter_showresets` / `_showheight` / `_showtime` / `_showavgtime` | `1` | Show each HUD line |
| `resetcounter_heightball` / `_heightcar` | `1` / `0` | Height source(s) |
| `resetcounter_keepcount_enabled` | `1` | Keep stats on screen after a reset |
| `resetcounter_keepcount_duration` | `3` | How long to keep them (seconds) |
| `resetcounter_message_enabled` | `0` | Append an encouragement message |
| `resetcounter_pb` | `0` | Personal best (most resets in a run) |
| `resetcounter_customtext_enabled` | `0` | Show the custom text overlay |
| `resetcounter_customtext` | `""` | Custom overlay text |
| `resetcounter_customtext_posx` / `_posy` / `_size` | `5` / `200` / `3` | Custom text placement |
| `resetcounter_resetcounter` | — | Command: manually zero the counter |

## Building

Requires Visual Studio with the C++ desktop workload and the BakkesMod SDK
installed (BakkesMod copies its SDK to
`%APPDATA%\bakkesmod\bakkesmod\bakkesmodsdk`). Open `ResetCounterPlugin.slnx`
(or the `.vcxproj`), select **Release | x64**, and build. The post-build step
copies the DLL into your BakkesMod `plugins` folder automatically.

See [`ResetCounterPlugin/BUILD_AND_INSTALL.md`](ResetCounterPlugin/BUILD_AND_INSTALL.md)
for details and environment notes.

## Installing a built DLL manually

1. In the BakkesMod console (F6): `plugin unload resetcounterplugin`
2. Copy `ResetCounterPlugin/plugins/ResetCounterPlugin.dll` to
   `%APPDATA%\bakkesmod\bakkesmod\plugins\`
3. `plugin load resetcounterplugin`

## License

[MIT](LICENSE) © 2026 Renan Zortea

Includes [Dear ImGui](https://github.com/ocornut/imgui) and parts of the
[Martinii89 BakkesMod plugin template](https://github.com/Martinii89/BakkesmodPluginTemplate),
both MIT licensed.
