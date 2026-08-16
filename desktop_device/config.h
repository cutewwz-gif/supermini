#pragma once

#include "secrets.h"

// ---- Board / TFT pins (ESP32-C3 SuperMini) ----
static const int PIN_TFT_CS   = 10;
static const int PIN_TFT_DC   = 5;
static const int PIN_TFT_RST  = 21;
static const int PIN_TFT_MOSI = 7;
static const int PIN_TFT_SCLK = 6;

// Landscape opposite of previous demo (screen was upside-down).
static const uint8_t TFT_ROTATION = 3;

// Buttons: active LOW with INPUT_PULLUP
static const int PIN_BTN_K1 = 0;
static const int PIN_BTN_K2 = 1;
static const int PIN_BTN_K3 = 2;
static const int PIN_BTN_K4 = 3;

// ---- Guangzhou ----
static const float CITY_LAT = 23.1291f;
static const float CITY_LON = 113.2644f;
static const char *CITY_NAME = "Guangzhou";
static const char *TZ_INFO = "CST-8";  // China Standard Time

// ---- Timing ----
static const uint32_t WEATHER_REFRESH_MS = 10UL * 60UL * 1000UL;  // when weather Auto
static const uint32_t IMAGE_REFRESH_MS = 30UL * 1000UL;           // when images Auto + on module
static const uint32_t HYPIXEL_REFRESH_MS = 45UL * 1000UL;         // when hypixel Auto + on module
static const uint32_t UI_TICK_MS = 250;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

// ---- SuperMini companion API (host/key live in secrets.h) ----
// Device talks only to this hub over WiFi; offline dorm: OPPO Watch BLE proxy.
#ifndef SUPERMINI_API_BASE
#define SUPERMINI_API_BASE "http://127.0.0.1/supermini"
#endif
#ifndef SUPERMINI_HUB_HOST
#define SUPERMINI_HUB_HOST "hub"
#endif
static const char *SUPERMINI_API_WEATHER = SUPERMINI_API_BASE "/api/weather";
static const char *SUPERMINI_API_IMAGES = SUPERMINI_API_BASE "/api/images";
static const char *SUPERMINI_API_RGB565_FMT = SUPERMINI_API_BASE "/api/images/%s/rgb565";
static const char *SUPERMINI_API_HYPIXEL = SUPERMINI_API_BASE "/api/hypixel";
