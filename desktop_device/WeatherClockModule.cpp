#include "WeatherClockModule.h"
#include "WeatherIcons.h"
#include "UiChrome.h"
#include "config.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

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

void formatClockLines(AppContext &ctx, char *lineTime, size_t timeLen,
                      char *lineDate, size_t dateLen) {
  struct tm tinfo = {};
  if (ctx.net.timeSynced() && getLocalTime(&tinfo, 0)) {
    snprintf(lineTime, timeLen, "%02d:%02d:%02d",
             tinfo.tm_hour, tinfo.tm_min, tinfo.tm_sec);
    snprintf(lineDate, dateLen, "%04d-%02d-%02d",
             tinfo.tm_year + 1900, tinfo.tm_mon + 1, tinfo.tm_mday);
  } else {
    snprintf(lineTime, timeLen, "--:--:--");
    snprintf(lineDate, dateLen, "syncing NTP...");
  }
}

// Compact tip icons under weather glyph (~14×12)
void drawUmbrellaIcon(Adafruit_GFX &g, int16_t x, int16_t y, uint16_t color) {
  // Light canopy: single arc polyline (not a filled blob)
  g.drawPixel(x + 1, y + 4, color);
  g.drawPixel(x + 2, y + 3, color);
  g.drawPixel(x + 3, y + 2, color);
  g.drawPixel(x + 4, y + 2, color);
  g.drawPixel(x + 5, y + 1, color);
  g.drawPixel(x + 6, y + 1, color);
  g.drawPixel(x + 7, y + 1, color);
  g.drawPixel(x + 8, y + 2, color);
  g.drawPixel(x + 9, y + 2, color);
  g.drawPixel(x + 10, y + 3, color);
  g.drawPixel(x + 11, y + 4, color);
  // hem chord
  g.drawFastHLine(x + 1, y + 5, 11, color);
  // shaft + small J hook
  g.drawFastVLine(x + 6, y + 5, 5, color);
  g.drawPixel(x + 7, y + 10, color);
  g.drawPixel(x + 8, y + 10, color);
}

void drawHatIcon(Adafruit_GFX &g, int16_t x, int16_t y, uint16_t color) {
  // 鸭舌帽 as one connected silhouette: crown → continuous bill
  // crown outline
  g.drawPixel(x + 3, y + 2, color);
  g.drawPixel(x + 4, y + 1, color);
  g.drawPixel(x + 5, y + 1, color);
  g.drawPixel(x + 6, y + 1, color);
  g.drawPixel(x + 7, y + 2, color);
  g.drawFastVLine(x + 2, y + 3, 4, color);  // back of crown
  g.drawFastVLine(x + 8, y + 2, 4, color);  // front of crown (meets bill)
  g.drawFastHLine(x + 3, y + 6, 5, color);  // crown bottom / joins bill
  // bill extends forward from crown front — same band, not floating
  g.drawFastHLine(x + 8, y + 5, 5, color);  // top of bill (from crown front)
  g.drawFastHLine(x + 8, y + 6, 6, color);  // bill body continuous with crown bottom
  g.drawPixel(x + 13, y + 5, color);        // bill tip
  g.drawPixel(x + 12, y + 4, color);        // slight curve up at tip
}

}  // namespace

void WeatherClockModule::syncSnapshot(AppContext &ctx) {
  struct tm tinfo = {};
  const bool hasTime = ctx.net.timeSynced() && getLocalTime(&tinfo, 0);
  _lastSec = hasTime ? tinfo.tm_sec : (int)((millis() / 1000) % 60);
  _lastYday = hasTime ? tinfo.tm_yday : -1;
  _lastWifi = ctx.net.wifiConnected();
  const auto &wx = ctx.net.weather();
  _lastWeatherValid = wx.valid;
  _lastTemp = wx.temperatureC;
  _lastHum = wx.humidity;
  _lastCode = wx.weatherCode;
  _lastTipUmbrella = wx.tipUmbrella;
  _lastTipHat = wx.tipHat;
  _lastFetchMs = wx.fetchedAtMs;
  _lastBleMs = ctx.ble.lastCommitMs();
  _lastBleConn = ctx.ble.clientConnected();
  _lastFlash = ctx.activeFlashKey();
}

void WeatherClockModule::onEnter(AppContext &ctx) {
  if (!ctx.net.weather().valid) {
    ctx.net.requestWeatherRefresh();
  }
  syncSnapshot(ctx);
  paint(ctx);
  _lastUiMs = millis();
}

void WeatherClockModule::onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) {
  if (!pressed) return;
  // K1 = manual refresh all weather/forecast (ModuleManager routes home K1 here)
  if (btnIndex == 0) {
    ctx.net.requestWeatherRefresh();
  }
}

void WeatherClockModule::paintTimeOnly(AppContext &ctx) {
  char lineTime[16];
  char lineDate[20];
  formatClockLines(ctx, lineTime, sizeof(lineTime), lineDate, sizeof(lineDate));
  auto &tft = ctx.display.tft();
  padPrint(tft, 16, 16, 2, ST77XX_CYAN, ST77XX_BLACK, lineTime, 8);
}

void WeatherClockModule::update(AppContext &ctx) {
  const uint32_t now = millis();
  if (now - _lastUiMs < UI_TICK_MS) return;
  _lastUiMs = now;

  struct tm tinfo = {};
  const bool hasTime = ctx.net.timeSynced() && getLocalTime(&tinfo, 0);
  const int sec = hasTime ? tinfo.tm_sec : (int)((now / 1000) % 60);
  const int yday = hasTime ? tinfo.tm_yday : -2;
  const bool wifi = ctx.net.wifiConnected();
  const auto &wx = ctx.net.weather();
  const int8_t flash = ctx.activeFlashKey();
  const bool night = ctx.net.isNightNow();
  static bool lastNight = false;
  static bool lastSunriseValid = false;
  static uint8_t lastRiseH = 255, lastRiseM = 255;

  const auto &as = ctx.net.astro();
  const bool sunriseChanged =
      (as.nextSunriseValid != lastSunriseValid) ||
      (as.nextSunriseValid && (as.nextH != lastRiseH || as.nextM != lastRiseM));

  const bool weatherChanged =
      (wx.valid != _lastWeatherValid) ||
      (wx.fetchedAtMs != _lastFetchMs) ||
      (wx.valid && (_lastCode != wx.weatherCode ||
                    !isfinite(_lastTemp) || fabsf(wx.temperatureC - _lastTemp) > 0.05f ||
                    !isfinite(_lastHum) || fabsf(wx.humidity - _lastHum) > 0.5f ||
                    wx.tipUmbrella != _lastTipUmbrella ||
                    wx.tipHat != _lastTipHat));

  const bool bleChanged =
      (ctx.ble.clientConnected() != _lastBleConn) ||
      (ctx.ble.lastCommitMs() != _lastBleMs);

  const bool structural =
      (wifi != _lastWifi) || weatherChanged || bleChanged || (yday != _lastYday) ||
      (flash != _lastFlash) || (night != lastNight) || sunriseChanged;

  if (structural) {
    _lastSec = sec;
    _lastYday = yday;
    _lastWifi = wifi;
    _lastWeatherValid = wx.valid;
    _lastTemp = wx.temperatureC;
    _lastHum = wx.humidity;
    _lastCode = wx.weatherCode;
    _lastTipUmbrella = wx.tipUmbrella;
    _lastTipHat = wx.tipHat;
    _lastFetchMs = wx.fetchedAtMs;
    _lastBleMs = ctx.ble.lastCommitMs();
    _lastBleConn = ctx.ble.clientConnected();
    _lastFlash = flash;
    lastNight = night;
    lastSunriseValid = as.nextSunriseValid;
    lastRiseH = as.nextH;
    lastRiseM = as.nextM;
    paint(ctx);
    return;
  }

  if (sec != _lastSec) {
    _lastSec = sec;
    paintTimeOnly(ctx);
  }
}

void WeatherClockModule::paint(AppContext &ctx) {
  ctx.display.beginFrame(ST77XX_BLACK);
  Adafruit_GFX &g = ctx.display.gfx();

  g.fillRect(0, 0, g.width(), 12, 0x2104);
  padPrint(g, 2, 2, 1, ST77XX_WHITE, 0x2104, CITY_NAME, 12);

  char lineTime[16];
  char lineDate[20];
  formatClockLines(ctx, lineTime, sizeof(lineTime), lineDate, sizeof(lineDate));
  padPrint(g, 16, 16, 2, ST77XX_CYAN, ST77XX_BLACK, lineTime, 8);
  padPrint(g, 16, 38, 1, ST77XX_WHITE, ST77XX_BLACK, lineDate, 16);

  const auto &wx = ctx.net.weather();
  const bool night = ctx.net.isNightNow();

  if (!ctx.net.wifiConnected()) {
    padPrint(g, 8, 58, 1, ST77XX_YELLOW, ST77XX_BLACK, "WiFi disconnected", 20);
  } else if (!wx.valid) {
    padPrint(g, 8, 58, 1, ST77XX_YELLOW, ST77XX_BLACK, "Fetching weather...", 20);
  } else {
    drawWeatherIcon(g, 6, 50, 30, wx.weatherCode, night);

    // Tip icons under weather glyph: umbrella | hat
    const uint16_t dim = 0x4208;   // dark gray when not needed
    const uint16_t lit = 0xFD20;   // orange when needed
    drawUmbrellaIcon(g, 4, 82, wx.tipUmbrella ? lit : dim);
    drawHatIcon(g, 20, 82, wx.tipHat ? lit : dim);

    char tempLine[16];
    snprintf(tempLine, sizeof(tempLine), "%.1f C", wx.temperatureC);
    padPrint(g, 42, 52, 2, ST77XX_GREEN, ST77XX_BLACK, tempLine, 7);

    padPrint(g, 42, 72, 1, ST77XX_WHITE, ST77XX_BLACK,
             weatherIconEnglishShort(wx.weatherCode, night), 12);

    char meta[28];
    snprintf(meta, sizeof(meta), "H:%.0f%% W:%.1f", wx.humidity, wx.windMs);
    padPrint(g, 42, 86, 1, 0xC618, ST77XX_BLACK, meta, 16);
  }

  // Always show next sunrise (tips are icons now)
  const auto &as = ctx.net.astro();
  char riseLine[28];
  if (as.nextSunriseValid) {
    struct tm nowTm = {};
    getLocalTime(&nowTm, 0);
    const bool tomorrow = (as.nextDy != (uint8_t)nowTm.tm_mday) ||
                          (as.nextMo != (uint8_t)(nowTm.tm_mon + 1));
    if (tomorrow) {
      snprintf(riseLine, sizeof(riseLine), "Sunrise %02u-%02u %02u:%02u",
               as.nextMo, as.nextDy, as.nextH, as.nextM);
    } else {
      snprintf(riseLine, sizeof(riseLine), "Sunrise %02u:%02u", as.nextH, as.nextM);
    }
  } else {
    snprintf(riseLine, sizeof(riseLine), "Sunrise --:--");
  }
  padPrint(g, 8, 100, 1, ST77XX_YELLOW, ST77XX_BLACK, riseLine, 22);

  UiChrome::drawStatusBar(g, ctx);
  ctx.display.endFrame();
}
