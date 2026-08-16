#include "ButtonsHal.h"

void ButtonsHal::begin() {
  for (uint8_t i = 0; i < kCount; i++) {
    pinMode(_pins[i], INPUT_PULLUP);
    _stable[i] = digitalRead(_pins[i]);
    _pressedEvent[i] = false;
    _longEvent[i] = false;
    _longFired[i] = false;
    _downSince[i] = 0;
    _nextRepeatMs[i] = 0;
    _lastChange[i] = millis();
  }
}

void ButtonsHal::update() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < kCount; i++) {
    _pressedEvent[i] = false;
    _longEvent[i] = false;

    const bool raw = digitalRead(_pins[i]);
    if (raw != _stable[i] && (now - _lastChange[i]) > kDebounceMs) {
      _lastChange[i] = now;
      const bool was = _stable[i];
      _stable[i] = raw;
      // INPUT_PULLUP: pressed = LOW
      if (was == HIGH && raw == LOW) {
        _downSince[i] = now;
        _longFired[i] = false;
        _pressedEvent[i] = true;  // immediate (incl. K4 next-module)
        _nextRepeatMs[i] = now + kRepeatInitialMs;
      } else if (was == LOW && raw == HIGH) {
        _downSince[i] = 0;
        _nextRepeatMs[i] = 0;
      }
    }

    // Hold-to-repeat: K1/K2 pages + menu; K3 menu-down / next-module.
    // K4 stays one-shot so hold-for-standby does not spam.
    if (i <= 2 && _stable[i] == LOW && !_longFired[i] &&
        _nextRepeatMs[i] != 0 && (int32_t)(now - _nextRepeatMs[i]) >= 0) {
      _pressedEvent[i] = true;
      _nextRepeatMs[i] = now + kRepeatEveryMs;
    }

    // K4 long-press → standby (stops further repeats)
    if (i == 3 && _stable[i] == LOW && !_longFired[i] && _downSince[i] != 0 &&
        (now - _downSince[i]) >= kLongMs) {
      _longFired[i] = true;
      _longEvent[i] = true;
      _pressedEvent[i] = false;  // this tick is long, not short
      _nextRepeatMs[i] = 0;
    }
  }
}

bool ButtonsHal::wasPressed(uint8_t index) const {
  if (index >= kCount) return false;
  return _pressedEvent[index];
}

bool ButtonsHal::wasLongPressed(uint8_t index) const {
  if (index >= kCount) return false;
  return _longEvent[index];
}

bool ButtonsHal::isDown(uint8_t index) const {
  if (index >= kCount) return false;
  return _stable[index] == LOW;
}
