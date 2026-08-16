#include "WeatherForecastModule.h"
#include "WeatherIcons.h"
#include "UiChrome.h"
#include "config.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr uint8_t kRowsPerPage = 5;
constexpr uint8_t kDailyNearCount = 4;  // nearer days on page -1
constexpr int16_t kIconSize = 12;
constexpr int16_t kIconX = 82;
constexpr int16_t kPopX = 100;  // aligned P% column (4 chars: " 80%")

void padPrint(Adafruit_GFX &g, int16_t x, int16_t y, uint8_t size,
              uint16_t fg, uint16_t bg, const char *text, uint8_t fieldChars) {
  char buf[40];
  if (fieldChars >= sizeof(buf)) fieldChars = sizeof(buf) - 1;
  snprintf(buf, sizeof(buf), "%-*s", fieldChars, text ? text : "");
  g.setTextSize(size);
  g.setTextColor(fg, bg);
  g.setCursor(x, y);
  g.print(buf);
}

}  // namespace

bool WeatherForecastModule::hourIsNight(AppContext &ctx, uint8_t month, uint8_t day,
                                        uint8_t hour) {
  struct tm slot = {};
  struct tm nowTm = {};
  if (!getLocalTime(&nowTm, 0)) return (hour >= 19 || hour < 6);
  slot = nowTm;
  slot.tm_mon = month - 1;
  slot.tm_mday = day;
  slot.tm_hour = hour;
  slot.tm_min = 30;
  slot.tm_sec = 0;
  // Handle year wrap roughly
  if (month < (nowTm.tm_mon + 1) && (nowTm.tm_mon + 1) == 12) {
    slot.tm_year = nowTm.tm_year + 1;
  }
  const time_t ep = mktime(&slot);
  return ctx.net.isNightAt(ep);
}

uint8_t WeatherForecastModule::hourlyPages(AppContext &ctx) const {
  const uint8_t n = ctx.net.forecast().hourlyCount;
  return n == 0 ? 1 : (uint8_t)((n + kRowsPerPage - 1) / kRowsPerPage);
}

uint8_t WeatherForecastModule::dailyPages(AppContext &ctx) const {
  return ctx.net.forecast().dailyCount > 0 ? 2 : 0;
}

uint8_t WeatherForecastModule::totalPages(AppContext &ctx) const {
  // Numerical order: daily -2, daily -1, then hourly 1..N
  return (uint8_t)(dailyPages(ctx) + hourlyPages(ctx));
}

void WeatherForecastModule::clampPage(AppContext &ctx) {
  const uint8_t pages = totalPages(ctx);
  if (_page >= pages) _page = pages ? (uint8_t)(pages - 1) : 0;
}

void WeatherForecastModule::onEnter(AppContext &ctx) {
  _preferNearDaily = true;
  // Default to daily -1 (near days). Order remains -2, -1, hourly…
  const uint8_t dPages = dailyPages(ctx);
  _page = (dPages >= 2) ? 1 : 0;
  // Lazy net: pull weather/forecast only when this module is opened.
  ctx.net.requestWeatherRefresh();
  _lastForecastMs = ctx.net.forecast().fetchedAtMs;
  _lastFlash = ctx.activeFlashKey();
  _lastWifi = ctx.net.wifiConnected();
  clampPage(ctx);
  paint(ctx);
  _dirty = false;
}

void WeatherForecastModule::onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) {
  if (!pressed) return;
  const uint8_t pages = totalPages(ctx);
  if (btnIndex == 0) {
    if (pages == 0) return;
    _preferNearDaily = false;
    _page = (_page + 1) % pages;
    _dirty = true;
  } else if (btnIndex == 1) {
    if (pages == 0) return;
    _preferNearDaily = false;
    _page = (_page + pages - 1) % pages;
    _dirty = true;
  } else if (btnIndex == 3) {
    ctx.net.requestWeatherRefresh();
    _dirty = true;
  }
}

void WeatherForecastModule::update(AppContext &ctx) {
  const auto &fc = ctx.net.forecast();
  const int8_t flash = ctx.activeFlashKey();
  const bool wifi = ctx.net.wifiConnected();

  if (fc.fetchedAtMs != _lastForecastMs) {
    _lastForecastMs = fc.fetchedAtMs;
    if (_preferNearDaily && dailyPages(ctx) >= 2) {
      _page = 1;  // -1
      _preferNearDaily = false;
    } else if (_preferNearDaily) {
      _preferNearDaily = false;
    }
    clampPage(ctx);
    _dirty = true;
  }
  // Status bar shows weather update time
  static uint32_t lastWxBar = 0;
  if (ctx.net.weather().fetchedAtMs != lastWxBar) {
    lastWxBar = ctx.net.weather().fetchedAtMs;
    _dirty = true;
  }
  if (flash != _lastFlash || wifi != _lastWifi) {
    _lastFlash = flash;
    _lastWifi = wifi;
    _dirty = true;
  }

  if (_dirty) {
    paint(ctx);
    _dirty = false;
  }
}

void WeatherForecastModule::paint(AppContext &ctx) {
  clampPage(ctx);
  ctx.display.beginFrame(ST77XX_BLACK);
  Adafruit_GFX &g = ctx.display.gfx();

  const uint8_t dPages = dailyPages(ctx);
  const uint8_t hPages = hourlyPages(ctx);
  const bool dailyMode = (_page < dPages);
  char modeName[10];
  char right[8];

  if (dailyMode) {
    snprintf(modeName, sizeof(modeName), "Daily");
    // page0 = -2 (far), page1 = -1 (near)
    snprintf(right, sizeof(right), "%d", _page == 0 ? -2 : -1);
  } else {
    const uint8_t local = (uint8_t)(_page - dPages);
    snprintf(modeName, sizeof(modeName), "Hourly");
    snprintf(right, sizeof(right), "%u/%u", (unsigned)(local + 1), (unsigned)hPages);
  }

  g.fillRect(0, 0, g.width(), 12, 0x2104);
  padPrint(g, 2, 2, 1, ST77XX_WHITE, 0x2104, modeName, 8);
  // "10/10" needs 5 glyphs × 6px = 30; old width-24 caused wrap of trailing 0
  padPrint(g, g.width() - 30, 2, 1, ST77XX_CYAN, 0x2104, right, 5);

  if (!ctx.net.wifiConnected()) {
    padPrint(g, 8, 40, 1, ST77XX_YELLOW, ST77XX_BLACK, "No WiFi", 18);
  } else if (!ctx.net.forecast().valid) {
    padPrint(g, 8, 40, 1, ST77XX_YELLOW, ST77XX_BLACK, "Loading...", 18);
  } else if (dailyMode) {
    if (_page == 0) paintDailyFar(ctx);   // -2
    else paintDailyNear(ctx);            // -1
  } else {
    paintHourly(ctx, (uint8_t)(_page - dPages));
  }

  UiChrome::drawStatusBar(g, ctx);
  ctx.display.endFrame();
}

void WeatherForecastModule::paintHourly(AppContext &ctx, uint8_t localPage) {
  Adafruit_GFX &g = ctx.display.gfx();
  const auto &fc = ctx.net.forecast();
  const uint8_t start = (uint8_t)(localPage * kRowsPerPage);

  padPrint(g, 2, 14, 1, 0x8410, ST77XX_BLACK, "MM-DD HH TMP", 12);
  padPrint(g, kPopX, 14, 1, 0x8410, ST77XX_BLACK, " P%", 4);

  for (uint8_t row = 0; row < kRowsPerPage; row++) {
    const uint8_t idx = start + row;
    const int16_t y = 26 + (int16_t)row * 14;
    if (y + 12 > UiChrome::contentBottom(g)) break;
    if (idx >= fc.hourlyCount) continue;

    const auto &p = fc.hourly[idx];
    char left[16];
    snprintf(left, sizeof(left), "%02u-%02u %02u %3d",
             p.month, p.day, p.hour, (int)p.tempC);
    padPrint(g, 2, y, 1, ST77XX_WHITE, ST77XX_BLACK, left, 12);

    const bool night = hourIsNight(ctx, p.month, p.day, p.hour);
    drawWeatherIcon(g, kIconX, y - 1, kIconSize, p.weatherCode, night);

    char pop[8];
    if (p.precipProb >= 0) snprintf(pop, sizeof(pop), "%3d%%", (int)p.precipProb);
    else snprintf(pop, sizeof(pop), " -- ");
    padPrint(g, kPopX, y, 1, ST77XX_WHITE, ST77XX_BLACK, pop, 4);
  }
}

void WeatherForecastModule::paintDailyNear(AppContext &ctx) {
  Adafruit_GFX &g = ctx.display.gfx();
  const auto &fc = ctx.net.forecast();

  padPrint(g, 2, 14, 1, 0x8410, ST77XX_BLACK, "MM-DD T", 8);
  padPrint(g, kPopX, 14, 1, 0x8410, ST77XX_BLACK, " P%", 4);

  const uint8_t end = fc.dailyCount < kDailyNearCount ? fc.dailyCount : kDailyNearCount;
  for (uint8_t idx = 0; idx < end; idx++) {
    const int16_t y = 26 + (int16_t)idx * 14;
    if (y + 12 > UiChrome::contentBottom(g)) break;
    const auto &p = fc.daily[idx];

    char left[16];
    snprintf(left, sizeof(left), "%02u-%02u %2d~%2d",
             p.month, p.day, (int)p.tMin, (int)p.tMax);
    padPrint(g, 2, y, 1, ST77XX_WHITE, ST77XX_BLACK, left, 12);
    drawWeatherIcon(g, kIconX, y - 1, kIconSize, p.weatherCode, false);

    char pop[8];
    if (p.precipProb >= 0) snprintf(pop, sizeof(pop), "%3d%%", (int)p.precipProb);
    else snprintf(pop, sizeof(pop), " -- ");
    padPrint(g, kPopX, y, 1, ST77XX_WHITE, ST77XX_BLACK, pop, 4);
  }
}

void WeatherForecastModule::paintDailyFar(AppContext &ctx) {
  Adafruit_GFX &g = ctx.display.gfx();
  const auto &fc = ctx.net.forecast();

  padPrint(g, 2, 14, 1, 0x8410, ST77XX_BLACK, "MM-DD T", 8);
  padPrint(g, kPopX, 14, 1, 0x8410, ST77XX_BLACK, " P%", 4);

  uint8_t row = 0;
  for (uint8_t idx = kDailyNearCount; idx < fc.dailyCount; idx++, row++) {
    const int16_t y = 26 + (int16_t)row * 14;
    if (y + 12 > UiChrome::contentBottom(g)) break;
    const auto &p = fc.daily[idx];

    char left[16];
    snprintf(left, sizeof(left), "%02u-%02u %2d~%2d",
             p.month, p.day, (int)p.tMin, (int)p.tMax);
    padPrint(g, 2, y, 1, ST77XX_WHITE, ST77XX_BLACK, left, 12);
    drawWeatherIcon(g, kIconX, y - 1, kIconSize, p.weatherCode, false);

    char pop[8];
    if (p.precipProb >= 0) snprintf(pop, sizeof(pop), "%3d%%", (int)p.precipProb);
    else snprintf(pop, sizeof(pop), " -- ");
    padPrint(g, kPopX, y, 1, ST77XX_WHITE, ST77XX_BLACK, pop, 4);
  }
}
