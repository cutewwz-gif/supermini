# SuperMini Watch Proxy (OPPO Watch SE / ColorOS Watch)

## Why not background?

Watches kill background apps. This app is **foreground-only**:

1. Dorm has no WiFi → open **SuperMini Proxy** on the watch  
2. Tap **Sync weather** (uses eSIM)  
3. Pushes JSON to ESP32 over BLE  
4. Leave the app — being killed is fine  

## Protocol

- Device name: `SuperMini`
- Service: `6bc80001-a1b2-c3d4-e5f6-000000000001`
- DATA (write): append UTF-8 JSON chunks  
- CTRL (write): `CLEAR` then chunks then `COMMIT`

## Build / install

1. Install [Android Studio](https://developer.android.com/studio)
2. Open folder `watch_proxy/`
3. Sync Gradle, build APK (Debug)
4. Watch: enable Developer options → USB debugging  
5. Connect watch cradle to PC, allow debugging  
6. `adb install -r app/build/outputs/apk/debug/app-debug.apk`

If install fails (signature / ABI): try `adb install --abi armeabi-v7a ...` or build with `abiFilters 'armeabi-v7a','arm64-v8a'`.

## Config

Copy `local.properties.example` to `local.properties` and set:

- `sdk.dir` — Android SDK path
- `supermini.hub` / `supermini.apiKey` — same as desktop `secrets.h`

## Next APIs

Same pattern can push Hypixel later (second COMMIT type). Keep payloads small — BLE is slow.
