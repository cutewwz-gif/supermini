#include "DisplayHal.h"
#include <new>

void DisplayHal::begin() {
  _tft.initR(INITR_BLACKTAB);
  _tft.setRotation(TFT_ROTATION);
  _tft.fillScreen(ST77XX_BLACK);
}

bool DisplayHal::beginFrame(uint16_t bg) {
  if (!_frame) {
    // 160x128 RGB565 ≈ 40KB; keep one canvas for the app lifetime.
    _frame = new (std::nothrow) GFXcanvas16(_tft.width(), _tft.height());
    if (!_frame || !_frame->getBuffer()) {
      delete _frame;
      _frame = nullptr;
      Serial.println("[disp] frame canvas alloc failed, direct draw");
      _tft.fillScreen(bg);
      _frameActive = false;
      return false;
    }
    Serial.printf("[disp] frame canvas %dx%d ok\n", _tft.width(), _tft.height());
  }
  _frame->fillScreen(bg);
  _frameActive = true;
  return true;
}

Adafruit_GFX &DisplayHal::gfx() {
  if (_frameActive && _frame) return *_frame;
  return _tft;
}

void DisplayHal::endFrame() {
  if (_frameActive && _frame && _frame->getBuffer()) {
    _tft.drawRGBBitmap(0, 0, _frame->getBuffer(), _frame->width(), _frame->height());
  }
  _frameActive = false;
}

void DisplayHal::clear(uint16_t color) {
  _tft.fillScreen(color);
}

void DisplayHal::statusBar(const char *left, const char *right, uint16_t bg) {
  _tft.fillRect(0, 0, _tft.width(), 12, bg);
  _tft.setTextSize(1);
  _tft.setTextColor(ST77XX_WHITE);
  _tft.setCursor(2, 2);
  _tft.print(left ? left : "");
  if (right) {
    int16_t x1, y1;
    uint16_t w, h;
    _tft.getTextBounds(right, 0, 0, &x1, &y1, &w, &h);
    _tft.setCursor(_tft.width() - (int16_t)w - 2, 2);
    _tft.print(right);
  }
}
