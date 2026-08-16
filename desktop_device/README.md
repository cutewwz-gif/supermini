# Modular dorm desktop (ESP32-C3 SuperMini)

**Agent / contributor note:** before UI work, read [`UI_CONTRACT.md`](UI_CONTRACT.md).

## Hardware
- ESP32-C3 SuperMini
- 1.8" 128x160 ST7735 TFT
- Buttons K1..K4 on GPIO 0..3 (`INPUT_PULLUP`)

### TFT wiring
| TFT | ESP32-C3 |
|-----|----------|
| GND | GND |
| VCC | 3.3V |
| SCL | GPIO6 |
| SDA | GPIO7 |
| CS  | GPIO10 |
| DC  | GPIO5 |
| RST | GPIO21 |
| BLK | 3.3V |

## Controls
| Key | Action |
|-----|--------|
| **K3 / K4** | Next / previous **module** · Hold K4 3s: **standby** |
| **K1 / K2** | Next / previous **page** · On **Clock**: K1 **refresh weather**, K2 **menu** |

### Menu
| Key | Action |
|-----|--------|
| **K4 / K3** | Up / down |
| **K1** | Confirm / enter |
| **K2** | Back |

### Settings
| Row | Values |
|-----|--------|
| Weather | Auto / Manual (includes Forecast) |
| Images | On / Off |
| Hypixel | On / Off |

Weather always refreshes at **05:45 / 08:00 / 10:00 / 21:00 / 23:00**; Auto adds **10 min** polling. Hypixel On = **45s** while active.

Status bar: WiFi · key · `#N` · `update:HH:MM`.

## Modules
| # | Name | Description |
|---|------|-------------|
| -1 | Help | Operations guide |
| 0 | Images | Gallery |
| 1 | Clock | Time + weather (home) |
| 2 | Forecast | Daily -2/-1 then hourly |
| 3 | Hypixel | Rank + BedWars |

## Companion API
- Copy `secrets.example.h` → `secrets.h` and set WiFi, hub URL, and API key.
- Device APIs need header `X-SuperMini-Key`.
- Weather / Hypixel / images are aggregated on the hub; the ESP does not call third-party APIs directly.
## Weather icon legend
See Help module / prior README table (Clear / 弯月 / Rain / …).

## Setup
```bat
arduino-cli compile --fqbn esp32:esp32:nologo_esp32c3_super_mini:PartitionScheme=huge_app desktop_device
arduino-cli upload -p COM5 --fqbn esp32:esp32:nologo_esp32c3_super_mini:PartitionScheme=huge_app desktop_device
```

BLE (Bluedroid) needs the **huge_app** partition (~3MB app). Default partition is too small.

## Offline dorm (OPPO Watch SE)

Watches kill background apps — sync is **manual / foreground only**:

1. Desk advertises BLE name `SuperMini` (not in standby)
2. Open watch app `watch_proxy/` → **Sync weather** (eSIM → hub → BLE)
3. Close watch app anytime

See [`watch_proxy/README.md`](../watch_proxy/README.md).
