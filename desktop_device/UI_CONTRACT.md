# Desktop Device UI Contract

**READ THIS FILE BEFORE ANY UI / MODULE / FIRMWARE CHANGE.**

Project path: `desktop_device/`

## Hard UI rules (non-negotiable)

1. **No ugly refresh**
   - Do not `fillScreen` / large `fillRect` then redraw text line-by-line on the live TFT.
   - Prefer off-screen `beginFrame()` / `endFrame()` for full-page paints.
   - For high-frequency updates (e.g. clock seconds), use **opaque text overwrite** on TFT only for the changed glyphs — never wipe the whole page every second.
2. **No double paint on enter**
   - After `onEnter` paints once, sync all “last*” / `fetchedAtMs` snapshots so the next `update()` does not immediately repaint the same frame.
3. **No control-hint footer**
   - Never show strings like `K1:mode K2:page` on module screens.
   - Bottom 12px is the **shared status bar** only (see below).
4. **Content must not draw into the status bar**
   - Usable content height: `0 .. height-13`. Status bar: `y = height-12`.

## Buttons (global)

| Key | Outside menu | Inside menu / settings |
|-----|--------------|------------------------|
| **K1** | Next page · **Clock: refresh weather** | **Confirm** / enter / toggle |
| **K2** | Prev page · **Clock: open menu** | **Back** |
| **K3** | **Next module** | Move **down** |
| **K4** | **Previous module** · **Standby** (hold 3s) | Move **up** |

## Menu / Settings

- Open from **Clock** with **K2**
- Menu: **K4** up · **K3** down · **K1** confirm · **K2** back
- Settings rows:
  - **Weather** Auto / Manual (Forecast shares this)
  - **Images** On / Off
  - **Hypixel** On / Off

## Weather refresh

- **Always** at local **05:45, 08:00, 10:00, 21:00, 23:00**
- **Auto**: also every **10 minutes**
- **Clock K1**: manual refresh (weather + forecast + tips)
- Hypixel **On**: every **45s** while on that module
- Images **On**: periodic refresh while on that module

## Status bar (bottom)

1. **W** + dot — WiFi **green** / **red**  
2. **B** + dot — BLE **blue** / **red**  
3. **Key** flash `K1`..`K4`  
4. **Module** `#N` only (no title)  
5. **`update:HH:MM`** — last weather fetch; briefly `BLE:ok` / `BLE:fail` after watch sync
3. **Module** `#N` only (no title)  
4. **`update:HH:MM`** — last weather fetch wall time

## Module index

| # | id | title | Role |
|---|----|-------|------|
| -1 | `readme` | Help | Operations guide + branding |
| 0 | `images` | Images | Gallery from companion API |
| 1 | `weather_clock` | Clock | Time + weather (home) |
| 2 | `weather_forecast` | Forecast | Daily -2/-1 then hourly 1..N |
| 3 | `hypixel` | Hypixel | Rank + BedWars |

Boot opens **#1 Clock**.

## Network modes (Settings)

- **Manual** (default): fetch on enter when cache empty / needed
- **Auto**: timed refresh while appropriate

Clock **onEnter** requests weather if cache invalid.

## Clock outing tips

Server `tips` for rest-of-**today**:

- **Umbrella**: wet now (rain codes), or later today hourly rain/drizzle/thunder / precipProb ≥ 40%, or today’s daily precipProb ≥ 50%
- **Hat**: only while daylight remains — UV ≥ 6, or UV ≥ 4 with clear/partly cloudy, or hot (≥30°C) with UV ≥ 3 (never night heat alone)

Clock draws **umbrella / hat icons** under the weather glyph (lit = needed, dim = not). Sunrise stays on the bottom content line.

## Weather icons / Forecast paging

- Forecast page order (numeric): **-2** → **-1** → hourly **1 / N** …
- **Default open page: -1** (nearer days)
- Page badge must fit `10/10` without wrapping (5 glyph field)
- K1/K2 hold-repeat for fast paging; image loads must not block the button path

## Architecture

- Device talks to the companion hub (`SUPERMINI_API_BASE` in `secrets.h`) over WiFi when available.
- **Offline dorm:** BLE peripheral `SuperMini` accepts weather JSON from watch app (`watch_proxy/`) — **foreground sync only** (watches kill background).
- `ModuleManager` owns menu + standby + module switch routing  
- Standby disables BLE advertising to save power  
- `SettingsStore` (NVS) for Manual/Auto  
- Secrets in `secrets.h`
- Flash with `PartitionScheme=huge_app` (BLE needs ~3MB app slot)
