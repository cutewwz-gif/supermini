#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Arduino.h>
#include <stdio.h>

enum class WxIcon : uint8_t {
  Clear = 0,
  Partly,
  Cloudy,
  Fog,
  Drizzle,
  Rain,
  Snow,
  Thunder,
  Unknown
};

inline WxIcon weatherIconKind(int code) {
  if (code == 0 || code == 1) return WxIcon::Clear;
  if (code == 2) return WxIcon::Partly;
  if (code == 3) return WxIcon::Cloudy;
  if (code == 45 || code == 48) return WxIcon::Fog;
  if (code >= 51 && code <= 57) return WxIcon::Drizzle;
  if (code >= 61 && code <= 67) return WxIcon::Rain;
  if (code >= 71 && code <= 77) return WxIcon::Snow;
  if (code >= 80 && code <= 82) return WxIcon::Rain;
  if (code >= 95 && code <= 99) return WxIcon::Thunder;
  return WxIcon::Unknown;
}

inline const char *weatherIconEnglish(int code) {
  switch (weatherIconKind(code)) {
    case WxIcon::Clear: return "Clear";
    case WxIcon::Partly: return "Partly cloudy";
    case WxIcon::Cloudy: return "Overcast";
    case WxIcon::Fog: return "Fog";
    case WxIcon::Drizzle: return "Drizzle";
    case WxIcon::Rain: return "Rain";
    case WxIcon::Snow: return "Snow";
    case WxIcon::Thunder: return "Thunder";
    default: return "Unknown";
  }
}

inline const char *weatherIconChinese(int code) {
  switch (weatherIconKind(code)) {
    case WxIcon::Clear: return "晴";
    case WxIcon::Partly: return "多云";
    case WxIcon::Cloudy: return "阴";
    case WxIcon::Fog: return "雾";
    case WxIcon::Drizzle: return "毛毛雨";
    case WxIcon::Rain: return "雨";
    case WxIcon::Snow: return "雪";
    case WxIcon::Thunder: return "雷暴";
    default: return "未知";
  }
}

// Compact English for tight layouts
inline const char *weatherIconEnglishShort(int code, bool night = false) {
  switch (weatherIconKind(code)) {
    case WxIcon::Clear: return night ? "Clear night" : "Clear";
    case WxIcon::Partly: return night ? "Cloudy night" : "P.Cloudy";
    case WxIcon::Cloudy: return "Overcast";
    case WxIcon::Fog: return "Fog";
    case WxIcon::Drizzle: return "Drizzle";
    case WxIcon::Rain: return "Rain";
    case WxIcon::Snow: return "Snow";
    case WxIcon::Thunder: return "Thunder";
    default: return "Unknown";
  }
}

inline void drawWeatherIcon(Adafruit_GFX &g, int16_t x, int16_t y, uint8_t s, int code,
                            bool night = false) {
  if (s < 10) s = 10;
  const WxIcon kind = weatherIconKind(code);
  const int16_t cx = x + s / 2;
  const int16_t cy = y + s / 2;

  auto drawSun = [&](int16_t sx, int16_t sy, int16_t rad) {
    g.fillCircle(sx, sy, rad, ST77XX_YELLOW);
    const int16_t ray = rad + (s >= 20 ? 4 : 2);
    const int8_t dirs[8][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0},
                               {1, -1}, {1, 1}, {-1, -1}, {-1, 1}};
    for (uint8_t i = 0; i < 8; i++) {
      const int16_t ox = dirs[i][0];
      const int16_t oy = dirs[i][1];
      g.drawLine(sx + ox * (rad + 1), sy + oy * (rad + 1),
                 sx + ox * ray, sy + oy * ray, ST77XX_YELLOW);
    }
  };

  auto drawCrescent = [&](int16_t sx, int16_t sy, int16_t rad) {
    // True crescent (弯月): pixels inside outer disc but outside a shifted cut disc.
    if (rad < 3) rad = 3;
    const int16_t cutDx = (int16_t)(rad * 58 / 100);   // shift cut to the right
    const int16_t cutDy = (int16_t)(-rad * 12 / 100);  // slight up
    const int16_t cutR = (int16_t)(rad * 88 / 100);
    const uint16_t col = 0xFFE7;  // pale moon yellow
    const int32_t outerR2 = (int32_t)rad * rad;
    const int32_t cutR2 = (int32_t)cutR * cutR;
    for (int16_t dy = -rad; dy <= rad; dy++) {
      for (int16_t dx = -rad; dx <= rad; dx++) {
        const int32_t inOuter = (int32_t)dx * dx + (int32_t)dy * dy;
        if (inOuter > outerR2) continue;
        const int32_t cx = (int32_t)dx - cutDx;
        const int32_t cy = (int32_t)dy - cutDy;
        if (cx * cx + cy * cy <= cutR2) continue;
        g.drawPixel(sx + dx, sy + dy, col);
      }
    }
  };

  auto drawCloud = [&](int16_t ox, int16_t oy, uint16_t color) {
    const int16_t bodyY = oy + s * 55 / 100;
    const int16_t bodyH = s * 22 / 100;
    const int16_t bodyX = ox + s * 12 / 100;
    const int16_t bodyW = s * 76 / 100;
    g.fillRoundRect(bodyX, bodyY, bodyW, bodyH, bodyH / 2, color);
    g.fillCircle(ox + s * 30 / 100, oy + s * 52 / 100, s * 16 / 100, color);
    g.fillCircle(ox + s * 50 / 100, oy + s * 42 / 100, s * 20 / 100, color);
    g.fillCircle(ox + s * 70 / 100, oy + s * 52 / 100, s * 16 / 100, color);
  };

  auto drawDrop = [&](int16_t dx, int16_t dy, uint16_t color) {
    g.drawLine(dx, dy, dx - 1, dy + s * 18 / 100, color);
    g.drawPixel(dx - 1, dy + s * 18 / 100, color);
  };

  switch (kind) {
    case WxIcon::Clear:
      if (night) drawCrescent(cx, cy, (int16_t)(s * 28 / 100));
      else drawSun(cx, cy, (int16_t)(s * 22 / 100));
      break;

    case WxIcon::Partly:
      if (night) {
        // Cloud first, then crescent on top-left so the 弯月 stays visible.
        drawCloud(x, y + s * 10 / 100, 0xC618);
        {
          int16_t mr = (int16_t)(s * 18 / 100);
          if (mr < 4) mr = 4;
          drawCrescent(x + s * 22 / 100, y + s * 20 / 100, mr);
        }
      } else {
        drawSun(x + s * 28 / 100, y + s * 28 / 100, (int16_t)(s * 14 / 100));
        drawCloud(x, y + s * 8 / 100, 0xC618);
      }
      break;

    case WxIcon::Cloudy:
      drawCloud(x, y, 0x9CF3);
      break;

    case WxIcon::Fog:
      for (uint8_t i = 0; i < 4; i++) {
        const int16_t yy = y + s * (28 + i * 16) / 100;
        const int16_t inset = (i % 2) ? s * 12 / 100 : s * 6 / 100;
        g.drawFastHLine(x + inset, yy, s - inset * 2, 0xC618);
        g.drawFastHLine(x + inset, yy + 1, s - inset * 2, 0x8410);
      }
      break;

    case WxIcon::Drizzle:
      drawCloud(x, y - s * 6 / 100, 0x9CF3);
      drawDrop(x + s * 30 / 100, y + s * 78 / 100, ST77XX_CYAN);
      drawDrop(x + s * 50 / 100, y + s * 82 / 100, ST77XX_CYAN);
      drawDrop(x + s * 70 / 100, y + s * 78 / 100, ST77XX_CYAN);
      break;

    case WxIcon::Rain:
      drawCloud(x, y - s * 8 / 100, 0x7BEF);
      drawDrop(x + s * 28 / 100, y + s * 76 / 100, ST77XX_BLUE);
      drawDrop(x + s * 42 / 100, y + s * 84 / 100, ST77XX_BLUE);
      drawDrop(x + s * 56 / 100, y + s * 76 / 100, ST77XX_BLUE);
      drawDrop(x + s * 70 / 100, y + s * 84 / 100, ST77XX_BLUE);
      break;

    case WxIcon::Snow: {
      drawCloud(x, y - s * 8 / 100, 0xC618);
      const int16_t flakes[4][2] = {{35, 78}, {50, 86}, {65, 78}, {48, 72}};
      for (auto &f : flakes) {
        const int16_t fx = x + s * f[0] / 100;
        const int16_t fy = y + s * f[1] / 100;
        g.drawPixel(fx, fy, ST77XX_WHITE);
        g.drawPixel(fx + 1, fy, ST77XX_WHITE);
        g.drawPixel(fx, fy + 1, ST77XX_WHITE);
        g.drawPixel(fx - 1, fy, ST77XX_WHITE);
        g.drawPixel(fx, fy - 1, ST77XX_WHITE);
      }
      break;
    }

    case WxIcon::Thunder:
      drawCloud(x, y - s * 10 / 100, 0x8410);
      g.drawLine(cx + 1, y + s * 55 / 100, cx - 3, y + s * 72 / 100, ST77XX_YELLOW);
      g.drawLine(cx - 3, y + s * 72 / 100, cx + 2, y + s * 72 / 100, ST77XX_YELLOW);
      g.drawLine(cx + 2, y + s * 72 / 100, cx - 4, y + s * 95 / 100, ST77XX_YELLOW);
      g.drawLine(cx, y + s * 55 / 100, cx - 4, y + s * 72 / 100, ST77XX_YELLOW);
      break;

    default:
      g.drawCircle(cx, cy, s * 30 / 100, ST77XX_WHITE);
      g.setCursor(cx - 3, cy - 3);
      g.setTextSize(1);
      g.setTextColor(ST77XX_WHITE);
      g.print('?');
      break;
  }
}
