#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "config.h"

class DisplayHal {
public:
  void begin();
  Adafruit_ST7735 &tft() { return _tft; }

  // Off-screen compose then one blit (avoids black wipe + line-by-line text).
  // Falls back to direct TFT if heap cannot hold the canvas.
  bool beginFrame(uint16_t bg = ST77XX_BLACK);
  Adafruit_GFX &gfx();
  void endFrame();

  void clear(uint16_t color = ST77XX_BLACK);
  void statusBar(const char *left, const char *right, uint16_t bg = 0x2104);
  int16_t width() const { return _tft.width(); }
  int16_t height() const { return _tft.height(); }

private:
  Adafruit_ST7735 _tft = Adafruit_ST7735(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_MOSI, PIN_TFT_SCLK, PIN_TFT_RST);
  GFXcanvas16 *_frame = nullptr;
  bool _frameActive = false;
};
