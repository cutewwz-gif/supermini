/*
 * ESP32-C3 SuperMini + 1.8" 128x160 ST7735 TFT
 *
 * User wiring:
 *   TFT        ESP32-C3
 *   GND   ->   GND
 *   VCC   ->   3V3
 *   SCL   ->   GPIO6   (SCK)
 *   SDA   ->   GPIO7   (MOSI)
 *   CS    ->   GPIO10
 *   DC    ->   GPIO5
 *   RST   ->   GPIO21
 *   BLK   ->   3V3
 *
 * Optional buttons (INPUT_PULLUP): K1=0 K2=1 K3=2 K4=3
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS   10
#define TFT_DC   5
#define TFT_RST  21
#define TFT_MOSI 7
#define TFT_SCLK 6

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void drawHello() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(true);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(8, 20);
  tft.print("Hello!");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(8, 50);
  tft.print("ESP32-C3 SuperMini");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(8, 70);
  tft.print("1.8\" 128x160 TFT");

  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(8, 95);
  tft.print("ST7735 OK");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("ESP32-C3 ST7735 1.8 TFT hello");
  Serial.println("Pins: SCLK=6 MOSI=7 CS=10 DC=5 RST=21 BLK=3V3");

  // Most 1.8" 128x160 modules use BLACKTAB.
  // If colors/offset look wrong, try INITR_GREENTAB / INITR_REDTAB / INITR_18BLACKTAB.
  Serial.println("Init: INITR_BLACKTAB");
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);  // landscape 160x128
  tft.fillScreen(ST77XX_BLACK);

  drawHello();
  Serial.println("Display ready");
}

void loop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 1000) return;
  last = now;

  tft.fillRect(8, 110, 140, 12, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(8, 110);
  tft.printf("up %lu s", now / 1000);
}
