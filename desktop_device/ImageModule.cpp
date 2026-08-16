#include "ImageModule.h"
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

}  // namespace

void ImageModule::freeBuffer() {
  if (_pixels) {
    free(_pixels);
    _pixels = nullptr;
  }
  _imgOk = false;
  _loadedId[0] = '\0';
}

void ImageModule::syncChrome(AppContext &ctx) {
  _lastFlash = ctx.activeFlashKey();
  _lastWifi = ctx.net.wifiConnected();
}

void ImageModule::onEnter(AppContext &ctx) {
  syncChrome(ctx);
  _busy = false;
  _wantList = true;  // always refresh when entering
  _wantImage = false;
  paint(ctx);
  _dirty = false;

  // Fetch immediately so enter→image is one blocking path, not delayed ticks.
  if (ctx.net.wifiConnected()) {
    ensureList(ctx, true);
    if (_listOk && _count > 0 && !_imgOk) {
      loadCurrent(ctx);
    }
    if (_dirty) {
      paint(ctx);
      _dirty = false;
    }
  }
  syncChrome(ctx);
}

void ImageModule::onExit(AppContext &ctx) {
  (void)ctx;
  // Keep buffer across module switches to make re-entry instant when id unchanged.
}

void ImageModule::onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) {
  if (!pressed) return;

  if (btnIndex == 3) {  // K4 refresh list (only if somehow delivered; normally K4 = next module)
    _wantList = true;
    _imgOk = false;
    paint(ctx);
    if (ctx.net.wifiConnected()) {
      ensureList(ctx, true);
      if (_listOk && _count > 0) loadCurrent(ctx);
      if (_dirty) {
        paint(ctx);
        _dirty = false;
      }
    }
    syncChrome(ctx);
    return;
  }

  if (_count == 0) return;
  if (btnIndex == 0) {
    _index = (uint8_t)((_index + 1) % _count);
  } else if (btnIndex == 1) {
    _index = (uint8_t)((_index + _count - 1) % _count);
  } else {
    return;
  }

  if (_imgOk && _loadedId[0] && strcmp(_loadedId, _ids[_index]) == 0) {
    _dirty = true;
    paint(ctx);
    _dirty = false;
    return;
  }

  // Defer HTTP to update() so rapid K1/K2 page flips stay responsive.
  _imgOk = false;
  _wantImage = true;
  _dirty = true;
  paint(ctx);
  _dirty = false;
  syncChrome(ctx);
}

void ImageModule::update(AppContext &ctx) {
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
    const bool autoPoll = ctx.settings.imagesOn &&
                          (now - _lastFetchMs) > IMAGE_REFRESH_MS;
    if (_wantList || !_listOk || autoPoll) {
      ensureList(ctx, _wantList || !_listOk || autoPoll);
    }
    if (_wantImage || (_listOk && !_imgOk && _count > 0)) {
      loadCurrent(ctx);
    }
  }

  if (_dirty) {
    paint(ctx);
    _dirty = false;
  }
}

void ImageModule::ensureList(AppContext &ctx, bool force) {
  (void)ctx;
  if (!WiFi.isConnected()) return;
  _busy = true;
  _wantList = false;

  char prevId[16] = {};
  if (_count > 0 && _index < _count) {
    strncpy(prevId, _ids[_index], sizeof(prevId) - 1);
  }
  const bool hadImg = _imgOk && _loadedId[0];

  HTTPClient http;
  http.setTimeout(5000);
  http.setReuse(true);
  if (!http.begin(SUPERMINI_API_IMAGES)) {
    _busy = false;
    return;
  }
  http.addHeader("X-SuperMini-Key", SUPERMINI_API_KEY);
  const int code = http.GET();
  if (code != 200) {
    http.end();
    _busy = false;
    _dirty = true;
    return;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    _busy = false;
    return;
  }

  JsonArray arr = doc["images"].as<JsonArray>();
  _count = 0;
  for (JsonObject o : arr) {
    if (_count >= kMaxImages) break;
    const char *id = o["id"];
    if (!id || !id[0]) continue;
    strncpy(_ids[_count], id, sizeof(_ids[0]) - 1);
    _ids[_count][sizeof(_ids[0]) - 1] = '\0';
    _count++;
  }

  // Prefer staying on the same image id after refresh.
  uint8_t newIndex = 0;
  bool found = false;
  if (prevId[0]) {
    for (uint8_t i = 0; i < _count; i++) {
      if (strcmp(_ids[i], prevId) == 0) {
        newIndex = i;
        found = true;
        break;
      }
    }
  }
  _index = found ? newIndex : 0;
  _listOk = true;
  _lastFetchMs = millis();

  if (hadImg && found && strcmp(_loadedId, prevId) == 0) {
    _imgOk = true;
    _wantImage = false;
  } else if (_count > 0) {
    if (!hadImg || !found || strcmp(_loadedId, _ids[_index]) != 0) {
      _imgOk = false;
      _wantImage = true;
    }
  } else {
    _imgOk = false;
    _wantImage = false;
  }

  (void)force;
  _busy = false;
  _dirty = true;
}

bool ImageModule::loadCurrent(AppContext &ctx) {
  (void)ctx;
  _wantImage = false;
  if (_count == 0 || !WiFi.isConnected()) return false;
  if (_imgOk && strcmp(_loadedId, _ids[_index]) == 0) return true;

  _busy = true;

  if (!_pixels) {
    _pixels = (uint16_t *)malloc(kImgBytes);
    if (!_pixels) {
      _busy = false;
      _dirty = true;
      return false;
    }
  }

  char url[128];
  snprintf(url, sizeof(url), SUPERMINI_API_RGB565_FMT, _ids[_index]);

  HTTPClient http;
  http.setTimeout(8000);
  http.setReuse(true);
  if (!http.begin(url)) {
    _busy = false;
    return false;
  }
  http.addHeader("X-SuperMini-Key", SUPERMINI_API_KEY);
  const int code = http.GET();
  if (code != 200) {
    http.end();
    _busy = false;
    _dirty = true;
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t got = 0;
  uint8_t *dst = (uint8_t *)_pixels;
  const uint32_t start = millis();
  while (got < kImgBytes && (millis() - start) < 12000) {
    size_t avail = stream->available();
    if (avail == 0) {
      if (!http.connected()) break;
      delay(1);
      continue;
    }
    if (avail > (kImgBytes - got)) avail = kImgBytes - got;
    // Read in larger chunks to cut loop overhead on slow links.
    const size_t chunk = avail > 1024 ? 1024 : avail;
    const int n = stream->readBytes(dst + got, chunk);
    if (n <= 0) {
      delay(1);
      continue;
    }
    got += (size_t)n;
  }
  http.end();

  if (got == kImgBytes) {
    _imgOk = true;
    strncpy(_loadedId, _ids[_index], sizeof(_loadedId) - 1);
    _loadedId[sizeof(_loadedId) - 1] = '\0';
  } else {
    _imgOk = false;
    _loadedId[0] = '\0';
  }
  _busy = false;
  _dirty = true;
  return _imgOk;
}

void ImageModule::paint(AppContext &ctx) {
  ctx.display.beginFrame(ST77XX_BLACK);
  Adafruit_GFX &g = ctx.display.gfx();

  if (_imgOk && _pixels) {
    g.drawRGBBitmap(0, 0, _pixels, kImgW, kImgH);
    char badge[16];
    if (_count > 0) {
      snprintf(badge, sizeof(badge), "%u/%u", (unsigned)(_index + 1), (unsigned)_count);
    } else {
      snprintf(badge, sizeof(badge), "-/-");
    }
    g.fillRect(g.width() - 34, 0, 34, 12, 0x2104);
    g.setTextSize(1);
    g.setTextColor(ST77XX_CYAN, 0x2104);
    g.setCursor(g.width() - 32, 2);
    g.print(badge);
  } else {
    g.fillRect(0, 0, g.width(), 12, 0x2104);
    g.setTextSize(1);
    g.setTextColor(ST77XX_WHITE, 0x2104);
    g.setCursor(2, 2);
    g.print("Images");

    if (!ctx.net.wifiConnected()) {
      put(g, 8, 40, ST77XX_YELLOW, "No WiFi");
    } else if (_busy || _wantImage || !_listOk) {
      put(g, 8, 40, ST77XX_CYAN, "Loading...");
    } else if (_listOk && _count == 0) {
      put(g, 8, 34, ST77XX_WHITE, "No images yet");
      put(g, 8, 50, 0x8410, "Upload at");
      put(g, 8, 64, ST77XX_CYAN, SUPERMINI_HUB_HOST);
    } else {
      put(g, 8, 40, ST77XX_YELLOW, "Fetching...");
    }
  }

  UiChrome::drawStatusBar(g, ctx);
  ctx.display.endFrame();
}
