#include "ReadmeModule.h"
#include "UiChrome.h"
#include "WeatherIcons.h"
#include "config.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <stdio.h>

namespace {

void put(Adafruit_GFX &g, int16_t x, int16_t y, uint16_t fg, const char *s) {
  g.setTextSize(1);
  g.setTextColor(fg, ST77XX_BLACK);
  g.setCursor(x, y);
  g.print(s);
}

}  // namespace

void ReadmeModule::clampPage() {
  if (_page >= kPages) _page = kPages - 1;
}

void ReadmeModule::onEnter(AppContext &ctx) {
  _page = 0;
  _lastFlash = ctx.activeFlashKey();
  _lastWifi = ctx.net.wifiConnected();
  paint(ctx);
  _dirty = false;
}

void ReadmeModule::onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) {
  if (!pressed) return;
  if (btnIndex == 0) {  // K1 next page
    _page = (_page + 1) % kPages;
    _dirty = true;
  } else if (btnIndex == 1) {  // K2 prev page
    _page = (_page + kPages - 1) % kPages;
    _dirty = true;
  }
  (void)ctx;
}

void ReadmeModule::update(AppContext &ctx) {
  const int8_t flash = ctx.activeFlashKey();
  const bool wifi = ctx.net.wifiConnected();
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

void ReadmeModule::paint(AppContext &ctx) {
  clampPage();
  ctx.display.beginFrame(ST77XX_BLACK);
  Adafruit_GFX &g = ctx.display.gfx();

  char hdr[24];
  snprintf(hdr, sizeof(hdr), "Help %u/%u", (unsigned)(_page + 1), (unsigned)kPages);
  g.fillRect(0, 0, g.width(), 12, 0x2104);
  g.setTextSize(1);
  g.setTextColor(ST77XX_WHITE, 0x2104);
  g.setCursor(2, 2);
  g.print(hdr);

  if (_page == 0) {
    put(g, 8, 22, ST77XX_CYAN, "Dorm Desktop");
    put(g, 8, 36, ST77XX_WHITE, "ESP32-C3 + 1.8 TFT");
    put(g, 8, 54, ST77XX_YELLOW, "Powered by Cursor");
    put(g, 8, 72, 0x8410, "modular · tiny · cute");
    put(g, 8, 90, ST77XX_WHITE, "K1/K2: page");
  } else if (_page == 1) {
    put(g, 6, 16, ST77XX_CYAN, "Controls");
    put(g, 6, 30, ST77XX_WHITE, "K3 next / K4 prev");
    put(g, 6, 44, ST77XX_WHITE, "K1/K2 page");
    put(g, 6, 58, ST77XX_WHITE, "Home K2 menu");
    put(g, 6, 72, ST77XX_WHITE, "Home K1 refresh");
    put(g, 6, 86, ST77XX_WHITE, "Menu K1 ok K2 back");
    put(g, 6, 100, 0x8410, "Hold K4 sleep");
  } else if (_page == 2) {
    put(g, 6, 18, ST77XX_CYAN, "Modules");
    put(g, 6, 32, ST77XX_WHITE, "#-1 Help  (this)");
    put(g, 6, 44, ST77XX_WHITE, "#0 Images gallery");
    put(g, 6, 56, ST77XX_WHITE, "#1 Clock  time+now");
    put(g, 6, 68, ST77XX_WHITE, "#2 Forecast -2/-1/h");
    put(g, 6, 80, ST77XX_WHITE, "#3 Hypixel BW");
    put(g, 6, 96, 0x8410, "City: Guangzhou");
  } else {
    put(g, 4, 16, ST77XX_CYAN, "Weather icons");
    // Show day clear, night crescent, night cloudy, rain
    drawWeatherIcon(g, 4, 28, 14, 0, false);
    put(g, 22, 30, ST77XX_WHITE, "Clear");
    drawWeatherIcon(g, 4, 46, 14, 0, true);
    put(g, 22, 48, ST77XX_WHITE, "Clear night");
    drawWeatherIcon(g, 4, 64, 14, 2, true);
    put(g, 22, 66, ST77XX_WHITE, "Cloudy night");
    drawWeatherIcon(g, 4, 82, 14, 61, false);
    put(g, 22, 84, ST77XX_WHITE, "Rain");
  }

  UiChrome::drawStatusBar(g, ctx);
  ctx.display.endFrame();
}
