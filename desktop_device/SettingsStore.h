#pragma once

#include <Arduino.h>

// Net policy. Defaults save power.
struct SettingsStore {
  bool weatherAuto = false;  // Auto: 10min + schedule; Manual: schedule + Clock K1 only
  bool imagesOn = false;     // On: auto-refresh while on Images module
  bool hypixelOn = false;    // On: 45s refresh while on Hypixel module

  void begin();
  void load();
  void save() const;
};
