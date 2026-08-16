#include "SettingsStore.h"

#include <Preferences.h>

void SettingsStore::begin() {
  load();
}

void SettingsStore::load() {
  Preferences p;
  if (!p.begin("smset", true)) {
    return;
  }
  weatherAuto = p.getBool("wx", false) || p.getBool("fc", false);
  imagesOn = p.getBool("img", false);
  hypixelOn = p.getBool("hyp", false);
  p.end();
}

void SettingsStore::save() const {
  Preferences p;
  if (!p.begin("smset", false)) return;
  p.putBool("wx", weatherAuto);
  p.putBool("img", imagesOn);
  p.putBool("hyp", hypixelOn);
  p.end();
}
