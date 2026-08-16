#pragma once

#include "DisplayHal.h"
#include "ButtonsHal.h"
#include "NetServices.h"
#include "SettingsStore.h"
#include "BleBridge.h"

class AppContext {
public:
  DisplayHal display;
  ButtonsHal buttons;
  NetServices net;
  SettingsStore settings;
  BleBridge ble;

  // Filled by ModuleManager each tick / switch
  int8_t uiModuleIndex = 0;
  uint8_t uiModuleCount = 0;
  const char *uiModuleTitle = "";

  bool standby = false;

  void begin();
  void update();

  void notifyKey(uint8_t keyIndex);
  int8_t activeFlashKey() const;

  void enterStandby();
  void exitStandby();

private:
  int8_t _flashKey = -1;
  uint32_t _flashKeyUntil = 0;
};
