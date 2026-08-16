#pragma once

#include "IModule.h"
#include "AppContext.h"

class ModuleManager {
public:
  static const uint8_t kMaxModules = 8;

  void begin(AppContext *ctx);
  bool registerModule(IModule *mod);
  void setActive(uint8_t index);
  void update();
  void handleButtons();

  uint8_t count() const { return _count; }
  uint8_t activeIndex() const { return _active; }
  IModule *active() const;

private:
  enum class UiMode : uint8_t { Module, Menu, Settings };

  void publishUiMeta();
  void openMenu();
  void closeMenu();
  void openSettings();
  void paintMenu();
  void paintSettings();
  void wakeFromStandby();
  void switchModule(int8_t delta);

  AppContext *_ctx = nullptr;
  IModule *_mods[kMaxModules] = {};
  uint8_t _count = 0;
  uint8_t _active = 0;
  bool _entered = false;

  UiMode _mode = UiMode::Module;
  uint8_t _menuSel = 0;       // 0.._count-1 modules, _count = Settings
  uint8_t _settingsSel = 0;   // 0..2: Weather / Images / Hypixel
  bool _menuDirty = true;
};
