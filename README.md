# SuperMini

ESP32-C3 desk ornament: clock, weather, gallery, and Hypixel stats on a 1.8" ST7735. A companion hub caches third-party APIs; an optional watch app can push weather over BLE when dorm WiFi is down.

This repository is sanitized. WiFi passwords, hub hostnames, API keys, and deploy credentials stay on your machine (`desktop_device/secrets.h`, `watch_proxy/local.properties`, server env).

## Layout

| Path | What |
|------|------|
| `desktop_device/` | ESP32-C3 firmware (Arduino). Read `UI_CONTRACT.md` before UI changes. |
| `server_deploy/` | FastAPI companion hub (`/supermini`) + Hypixel edge helper |
| `watch_proxy/` | OPPO Watch SE foreground BLE weather proxy |
| `tools/` | Optional PC-side Hypixel relay if the hub cannot reach Cloudflare |

## Firmware

1. Copy `desktop_device/secrets.example.h` to `desktop_device/secrets.h` and fill WiFi + hub URL + API key.
2. Flash with the **huge_app** partition (BLE needs ~3MB):

```bat
arduino-cli compile --fqbn esp32:esp32:nologo_esp32c3_super_mini:PartitionScheme=huge_app desktop_device
arduino-cli upload -p COMx --fqbn esp32:esp32:nologo_esp32c3_super_mini:PartitionScheme=huge_app desktop_device
```

Details: [`desktop_device/README.md`](desktop_device/README.md)

## Hub

1. Copy `server_deploy/.env.example` and set `SUPERMINI_PASSWORD` (must match the device `SUPERMINI_API_KEY`).
2. If the hub cannot TLS to `api.hypixel.net`, run `hypixel_edge.py` on a host that can, and point `HYPIXEL_EDGE_URL` at it.
3. Put Hypixel API keys only on the hub config page — never in git.

## Watch proxy

Copy `watch_proxy/local.properties.example` to `watch_proxy/local.properties`. See [`watch_proxy/README.md`](watch_proxy/README.md).

## Later updates

```bat
git add -A
git status
git commit -m "your message"
git push
```

Keep `secrets.h`, `local.properties`, and `.env` untracked.
