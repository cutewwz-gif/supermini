#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <stdio.h>
#include <string.h>

#include "AppContext.h"

namespace UiChrome {

static constexpr int16_t kBarH = 12;

inline int16_t contentBottom(const Adafruit_GFX &g) {
  return g.height() - kBarH - 1;
}

inline void drawStatusBar(Adafruit_GFX &g, AppContext &ctx) {
  const int16_t y = g.height() - kBarH;
  const uint16_t bg = 0x2104;
  g.fillRect(0, y, g.width(), kBarH, bg);
  g.setTextSize(1);

  const bool wifi = ctx.net.wifiConnected();
  const bool bleOn = ctx.ble.clientConnected();
  const bool bleFresh =
      ctx.ble.lastCommitMs() != 0 &&
      (millis() - ctx.ble.lastCommitMs()) < 8000UL;

  // W = WiFi green/red · B = BLE blue/red
  g.setTextColor(ST77XX_WHITE, bg);
  g.setCursor(2, y + 2);
  g.print("W");
  g.fillCircle(14, y + 5, 3, wifi ? ST77XX_GREEN : ST77XX_RED);
  g.setCursor(22, y + 2);
  g.print("B");
  g.fillCircle(34, y + 5, 3, bleOn ? ST77XX_BLUE : ST77XX_RED);

  const int8_t k = ctx.activeFlashKey();
  char keyBuf[4] = "--";
  if (k >= 0 && k < 4) {
    keyBuf[0] = 'K';
    keyBuf[1] = (char)('1' + k);
    keyBuf[2] = '\0';
  }
  g.setTextColor(k >= 0 ? ST77XX_YELLOW : 0x8410, bg);
  g.setCursor(44, y + 2);
  g.print(keyBuf);

  // Module number only (no title) — leave room for update time
  char modBuf[6];
  if (ctx.uiModuleTitle && (strcmp(ctx.uiModuleTitle, "Menu") == 0)) {
    snprintf(modBuf, sizeof(modBuf), "#*");
  } else if (ctx.uiModuleTitle && (strcmp(ctx.uiModuleTitle, "Set") == 0)) {
    snprintf(modBuf, sizeof(modBuf), "#S");
  } else {
    snprintf(modBuf, sizeof(modBuf), "#%d", (int)ctx.uiModuleIndex);
  }
  g.setTextColor(ST77XX_CYAN, bg);
  g.setCursor(70, y + 2);
  g.print(modBuf);

  // Prefer update:HH:MM always when we have a stamp. Only flash BLE:fail briefly.
  char upd[16];
  uint16_t updColor = 0xC618;
  const auto &wx = ctx.net.weather();
  if (bleFresh && !ctx.ble.lastCommitOk()) {
    snprintf(upd, sizeof(upd), "BLE:fail");
    updColor = ST77XX_RED;
  } else if (wx.fetchedTimeValid) {
    snprintf(upd, sizeof(upd), "update:%02u:%02u", wx.fetchedHour, wx.fetchedMin);
    if (bleFresh && ctx.ble.lastCommitOk()) updColor = ST77XX_GREEN;
  } else {
    snprintf(upd, sizeof(upd), "update:--:--");
  }
  g.setTextColor(updColor, bg);
  g.setCursor(88, y + 2);
  g.print(upd);
}

}  // namespace UiChrome
