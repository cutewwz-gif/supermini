#include "AppContext.h"
#include <Adafruit_ST7735.h>

void AppContext::begin() {
  display.begin();
  buttons.begin();
  settings.begin();
  net.begin();
  ble.begin(&net);
}

void AppContext::update() {
  buttons.update();
  if (!standby) {
    net.setWeatherAuto(settings.weatherAuto);
    net.update();
    ble.update();
  }
}

void AppContext::notifyKey(uint8_t keyIndex) {
  if (keyIndex > 3) return;
  _flashKey = (int8_t)keyIndex;
  _flashKeyUntil = millis() + 600;
}

int8_t AppContext::activeFlashKey() const {
  for (uint8_t i = 0; i < 4; i++) {
    if (buttons.isDown(i)) return (int8_t)i;
  }
  if (_flashKey >= 0 && (int32_t)(millis() - _flashKeyUntil) < 0) {
    return _flashKey;
  }
  return -1;
}

void AppContext::enterStandby() {
  if (standby) return;
  standby = true;
  ble.setEnabled(false);  // save power; open watch app after wake if needed
  display.tft().enableDisplay(false);
  display.tft().fillScreen(ST77XX_BLACK);
}

void AppContext::exitStandby() {
  if (!standby) return;
  standby = false;
  display.tft().enableDisplay(true);
  ble.setEnabled(true);
}
