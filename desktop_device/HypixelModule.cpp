#include "HypixelModule.h"
#include "UiChrome.h"
#include "config.h"
#include "secrets.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace {

void put(Adafruit_GFX &g, int16_t x, int16_t y, uint16_t fg, const char *s) {
  g.setTextSize(1);
  g.setTextColor(fg, ST77XX_BLACK);
  g.setCursor(x, y);
  g.print(s);
}

uint16_t hex565(const char *hex) {
  if (!hex || strlen(hex) < 4) return ST77XX_WHITE;
  unsigned v = 0;
  for (int i = 0; i < 4; i++) {
    char c = hex[i];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
    else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
  }
  return (uint16_t)v;
}

void drawStar(Adafruit_GFX &g, int16_t x, int16_t y, uint16_t color) {
  g.drawPixel(x + 3, y + 0, color);
  g.drawPixel(x + 3, y + 1, color);
  g.drawFastHLine(x + 1, y + 2, 5, color);
  g.drawFastHLine(x + 0, y + 3, 7, color);
  g.drawPixel(x + 1, y + 4, color);
  g.drawPixel(x + 5, y + 4, color);
  g.drawPixel(x + 2, y + 4, color);
  g.drawPixel(x + 4, y + 4, color);
  g.drawPixel(x + 2, y + 5, color);
  g.drawPixel(x + 4, y + 5, color);
  g.drawPixel(x + 1, y + 6, color);
  g.drawPixel(x + 5, y + 6, color);
  g.drawPixel(x + 3, y + 5, color);
}

}  // namespace

uint16_t HypixelModule::colorFromName(const char *name) {
  if (!name) return ST77XX_WHITE;
  if (!strcmp(name, "aqua")) return 0x07FF;
  if (!strcmp(name, "green")) return 0x07E0;
  if (!strcmp(name, "gold")) return 0xFE00;
  if (!strcmp(name, "yellow")) return 0xFFE0;
  if (!strcmp(name, "red")) return 0xF800;
  if (!strcmp(name, "white")) return ST77XX_WHITE;
  if (!strcmp(name, "gray") || !strcmp(name, "grey")) return 0x8410;
  if (!strcmp(name, "dark_gray")) return 0x4208;
  if (!strcmp(name, "blue")) return 0x001F;
  if (!strcmp(name, "dark_aqua")) return 0x0451;
  if (!strcmp(name, "dark_green")) return 0x03E0;
  if (!strcmp(name, "light_purple")) return 0xF81F;
  if (!strcmp(name, "dark_purple")) return 0x9012;
  if (!strcmp(name, "dark_red")) return 0xA800;
  if (!strcmp(name, "black")) return 0x0000;
  return ST77XX_WHITE;
}

void HypixelModule::syncChrome(AppContext &ctx) {
  _lastFlash = ctx.activeFlashKey();
  _lastWifi = ctx.net.wifiConnected();
}

void HypixelModule::onEnter(AppContext &ctx) {
  syncChrome(ctx);
  _wantFetch = true;
  paint(ctx);
  _dirty = false;
  if (ctx.net.wifiConnected()) {
    ensureData(ctx, true);
    if (_dirty) {
      paint(ctx);
      _dirty = false;
    }
  }
  syncChrome(ctx);
}

void HypixelModule::onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) {
  if (!pressed) return;
  // K4 = refresh
  if (btnIndex == 3) {
    _wantFetch = true;
    ensureData(ctx, true);
    if (_dirty) {
      paint(ctx);
      _dirty = false;
    }
    syncChrome(ctx);
  }
}

void HypixelModule::update(AppContext &ctx) {
  const int8_t flash = ctx.activeFlashKey();
  const bool wifi = ctx.net.wifiConnected();
  static uint32_t lastWxBar = 0;
  if (flash != _lastFlash || wifi != _lastWifi ||
      ctx.net.weather().fetchedAtMs != lastWxBar) {
    lastWxBar = ctx.net.weather().fetchedAtMs;
    syncChrome(ctx);
    _dirty = true;
  }

  if (!_busy && wifi) {
    const uint32_t now = millis();
    const bool autoPoll = ctx.settings.hypixelOn && _snap.valid &&
                          (now - _lastFetchMs) > HYPIXEL_REFRESH_MS;
    if (_wantFetch || autoPoll) {
      ensureData(ctx, true);
    }
  }

  if (_dirty) {
    paint(ctx);
    _dirty = false;
  }
}

void HypixelModule::ensureData(AppContext &ctx, bool force) {
  (void)ctx;
  _wantFetch = false;
  if (!WiFi.isConnected()) return;
  if (!force && _snap.valid) return;
  _busy = true;
  fetchFromServer();
  _busy = false;
  _lastFetchMs = millis();
  _dirty = true;
}

bool HypixelModule::fetchFromServer() {
  HTTPClient http;
  http.setTimeout(8000);
  http.setReuse(true);
  if (!http.begin(SUPERMINI_API_HYPIXEL)) return false;
  http.addHeader("X-SuperMini-Key", SUPERMINI_API_KEY);
  const int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  if (!doc["ok"].as<bool>()) return false;

  Snapshot s;
  s.valid = true;
  s.fetchedAtMs = millis();
  s.online = doc["online"] | false;
  const char *act = doc["activity"] | "";
  strncpy(s.activity, act, sizeof(s.activity) - 1);

  s.segCount = 0;
  JsonArray segs = doc["rank"]["segments"].as<JsonArray>();
  for (JsonObject o : segs) {
    if (s.segCount >= kMaxSeg) break;
    const char *t = o["t"] | "";
    const char *c = o["c"] | "white";
    strncpy(s.segs[s.segCount].text, t, sizeof(s.segs[0].text) - 1);
    s.segs[s.segCount].text[sizeof(s.segs[0].text) - 1] = '\0';
    const char *hex = nullptr;
    if (!doc["colors"].isNull() && doc["colors"][c].is<const char *>()) {
      hex = doc["colors"][c];
    }
    s.segs[s.segCount].color = hex ? hex565(hex) : colorFromName(c);
    s.segCount++;
  }
  if (s.segCount == 0) {
    const char *nm = doc["name"] | "?";
    strncpy(s.segs[0].text, nm, sizeof(s.segs[0].text) - 1);
    s.segs[0].color = 0x8410;
    s.segCount = 1;
  }

  JsonObject bw = doc["bedwars"].as<JsonObject>();
  s.stars = bw["stars"] | 0;
  s.kills = bw["kills"] | 0;
  s.deaths = bw["deaths"] | 0;
  s.kdr = bw["kdr"] | 0.0;
  s.fk = bw["final_kills"] | 0;
  s.fd = bw["final_deaths"] | 0;
  s.fkdr = bw["fkdr"] | 0.0;
  s.wins = bw["wins"] | 0;
  s.losses = bw["losses"] | 0;
  s.beds = bw["beds_broken"] | 0;

  _snap = s;
  return true;
}

void HypixelModule::paint(AppContext &ctx) {
  ctx.display.beginFrame(ST77XX_BLACK);
  Adafruit_GFX &g = ctx.display.gfx();

  g.fillRect(0, 0, g.width(), 12, 0x2104);
  g.setTextSize(1);
  g.setTextColor(ST77XX_WHITE, 0x2104);
  g.setCursor(2, 2);
  g.print("Hypixel");

  if (!ctx.net.wifiConnected()) {
    put(g, 6, 40, ST77XX_YELLOW, "No WiFi");
  } else if (_busy && !_snap.valid) {
    put(g, 6, 40, ST77XX_CYAN, "Loading...");
  } else if (!_snap.valid) {
    put(g, 6, 28, ST77XX_YELLOW, "No data");
    put(g, 6, 44, 0x8410, "Set key at");
    put(g, 6, 58, ST77XX_CYAN, SUPERMINI_HUB_HOST);
  } else {
    char line[48];
    char starNum[8];
    snprintf(starNum, sizeof(starNum), "%d", _snap.stars);
    const int16_t starBlockW = (int16_t)(8 + (int)strlen(starNum) * 6);
    const int16_t nameMaxX = g.width() - starBlockW - 4;

    // Row 1: colored rank/name … ★stars (right)
    int16_t x = 2;
    const int16_t yName = 16;
    g.setTextSize(1);
    for (uint8_t i = 0; i < _snap.segCount; i++) {
      const int16_t need = (int16_t)(strlen(_snap.segs[i].text) * 6);
      if (x + need > nameMaxX) break;
      g.setTextColor(_snap.segs[i].color, ST77XX_BLACK);
      g.setCursor(x, yName);
      g.print(_snap.segs[i].text);
      x += need;
    }
    const int16_t starX = g.width() - starBlockW;
    drawStar(g, starX, yName, ST77XX_YELLOW);
    put(g, starX + 8, yName, ST77XX_YELLOW, starNum);

    // Row 2: Offline  OR  Online <activity>  (single line)
    if (_snap.online) {
      snprintf(line, sizeof(line), "On %s", _snap.activity);
      // fit ~26 chars
      if (strlen(line) > 26) line[26] = '\0';
      put(g, 2, 30, ST77XX_GREEN, line);
    } else {
      put(g, 2, 30, 0x8410, "Offline");
    }

    // Row 3–6: combat
    snprintf(line, sizeof(line), "K %d  D %d", _snap.kills, _snap.deaths);
    put(g, 2, 44, ST77XX_WHITE, line);

    snprintf(line, sizeof(line), "FK %d  FD %d", _snap.fk, _snap.fd);
    put(g, 2, 56, ST77XX_WHITE, line);

    snprintf(line, sizeof(line), "KDR %.2f  FKDR %.2f",
             (double)_snap.kdr, (double)_snap.fkdr);
    put(g, 2, 68, ST77XX_CYAN, line);

    const int games = _snap.wins + _snap.losses;
    float wr = 0.0f;
    if (games > 0) wr = (100.0f * (float)_snap.wins) / (float)games;
    snprintf(line, sizeof(line), "WR %.0f%%  %d/%d", (double)wr, _snap.wins, _snap.losses);
    put(g, 2, 80, ST77XX_GREEN, line);

    snprintf(line, sizeof(line), "Beds %d", _snap.beds);
    put(g, 2, 92, ST77XX_YELLOW, line);
  }

  UiChrome::drawStatusBar(g, ctx);
  ctx.display.endFrame();
}
