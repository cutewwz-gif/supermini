#pragma once

#include <Arduino.h>
#include "config.h"

class ButtonsHal {
public:
  void begin();
  void update();

  // Short press / key-repeat tick (K4: press edge + repeat; long-press separate)
  bool wasPressed(uint8_t index) const;
  // K4 only: fires once after held >= 3s
  bool wasLongPressed(uint8_t index) const;
  bool isDown(uint8_t index) const;

private:
  static const uint8_t kCount = 4;
  static const uint32_t kDebounceMs = 12;
  static const uint32_t kLongMs = 3000;
  static const uint32_t kRepeatInitialMs = 160;
  static const uint32_t kRepeatEveryMs = 85;

  const int _pins[kCount] = {PIN_BTN_K1, PIN_BTN_K2, PIN_BTN_K3, PIN_BTN_K4};
  bool _stable[kCount] = {true, true, true, true};  // pullup idle = HIGH
  bool _pressedEvent[kCount] = {};
  bool _longEvent[kCount] = {};
  bool _longFired[kCount] = {};
  uint32_t _downSince[kCount] = {};
  uint32_t _nextRepeatMs[kCount] = {};
  uint32_t _lastChange[kCount] = {};
};
