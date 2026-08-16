#include "ModuleManager.h"
#include "UiChrome.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <stdio.h>
#include <string.h>

namespace {

void put(Adafruit_GFX &g, int16_t x, int16_t y, uint16_t fg, uint16_t bg, const char *s) {
  g.setTextSize(1);
  g.setTextColor(fg, bg);
  g.setCursor(x, y);
  g.print(s);
}

}  // namespace

void ModuleManager::begin(AppContext *ctx) {
  _ctx = ctx;
  _entered = false;
  _mode = UiMode::Module;
}

bool ModuleManager::registerModule(IModule *mod) {
  if (!mod || _count >= kMaxModules) return false;
  _mods[_count++] = mod;
  return true;
}

IModule *ModuleManager::active() const {
  if (_count == 0) return nullptr;
  return _mods[_active];
}

void ModuleManager::publishUiMeta() {
  if (!_ctx) return;
  if (_mode == UiMode::Menu) {
    _ctx->uiModuleIndex = 0;
    _ctx->uiModuleTitle = "Menu";
    _ctx->uiModuleCount = _count;
    return;
  }
  if (_mode == UiMode::Settings) {
    _ctx->uiModuleIndex = 0;
    _ctx->uiModuleTitle = "Set";
    _ctx->uiModuleCount = _count;
    return;
  }
  IModule *mod = active();
  _ctx->uiModuleIndex = mod ? mod->number() : (int8_t)_active;
  _ctx->uiModuleCount = _count;
  _ctx->uiModuleTitle = mod ? mod->title() : "";
}

void ModuleManager::setActive(uint8_t index) {
  if (_count == 0 || !_ctx) return;
  if (index >= _count) index = 0;

  IModule *cur = active();
  if (_entered && cur) {
    cur->onExit(*_ctx);
  }

  _active = index;
  _entered = true;
  _mode = UiMode::Module;
  publishUiMeta();
  active()->onEnter(*_ctx);
}

void ModuleManager::openMenu() {
  _mode = UiMode::Menu;
  _menuSel = _active;  // highlight current module
  if (_menuSel > _count) _menuSel = 0;
  _menuDirty = true;
  publishUiMeta();
  paintMenu();
}

void ModuleManager::closeMenu() {
  _mode = UiMode::Module;
  publishUiMeta();
  if (active()) {
    active()->onEnter(*_ctx);  // restore module view
  }
}

void ModuleManager::openSettings() {
  _mode = UiMode::Settings;
  _settingsSel = 0;
  _menuDirty = true;
  publishUiMeta();
  paintSettings();
}

void ModuleManager::wakeFromStandby() {
  if (!_ctx || !_ctx->standby) return;
  _ctx->exitStandby();
  _mode = UiMode::Module;
  publishUiMeta();
  if (active()) active()->onEnter(*_ctx);
}

void ModuleManager::switchModule(int8_t delta) {
  if (_count == 0 || !_ctx) return;
  if (_mode != UiMode::Module) {
    _mode = UiMode::Module;
  }
  int next = (int)_active + (int)delta;
  while (next < 0) next += _count;
  while (next >= (int)_count) next -= _count;
  setActive((uint8_t)next);
}

void ModuleManager::paintMenu() {
  if (!_ctx) return;
  _ctx->display.beginFrame(ST77XX_BLACK);
  Adafruit_GFX &g = _ctx->display.gfx();

  g.fillRect(0, 0, g.width(), 12, 0x2104);
  put(g, 2, 2, ST77XX_WHITE, 0x2104, "Menu");

  // rows: modules + Settings
  const uint8_t rows = (uint8_t)(_count + 1);
  for (uint8_t i = 0; i < rows; i++) {
    const int16_t y = 16 + (int16_t)i * 14;
    if (y + 10 > UiChrome::contentBottom(g)) break;
    const bool sel = (i == _menuSel);
    const uint16_t bg = sel ? 0x02F0 : ST77XX_BLACK;
    const uint16_t fg = sel ? ST77XX_BLACK : ST77XX_WHITE;
    if (sel) g.fillRect(0, y - 2, g.width(), 14, bg);

    char line[22];
    if (i < _count) {
      snprintf(line, sizeof(line), "#%d %s", (int)_mods[i]->number(), _mods[i]->title());
    } else {
      snprintf(line, sizeof(line), "Settings");
    }
    put(g, 6, y, fg, bg, line);
  }

  UiChrome::drawStatusBar(g, *_ctx);
  _ctx->display.endFrame();
  _menuDirty = false;
}

void ModuleManager::paintSettings() {
  if (!_ctx) return;
  _ctx->display.beginFrame(ST77XX_BLACK);
  Adafruit_GFX &g = _ctx->display.gfx();

  g.fillRect(0, 0, g.width(), 12, 0x2104);
  put(g, 2, 2, ST77XX_WHITE, 0x2104, "Settings");

  const char *labels[3] = {"Weather", "Images", "Hypixel"};
  char values[3][8];
  snprintf(values[0], sizeof(values[0]), "%s",
           _ctx->settings.weatherAuto ? "Auto" : "Manual");
  snprintf(values[1], sizeof(values[1]), "%s",
           _ctx->settings.imagesOn ? "On" : "Off");
  snprintf(values[2], sizeof(values[2]), "%s",
           _ctx->settings.hypixelOn ? "On" : "Off");

  for (uint8_t i = 0; i < 3; i++) {
    const int16_t y = 18 + (int16_t)i * 18;
    const bool sel = (i == _settingsSel);
    const uint16_t bg = sel ? 0x02F0 : ST77XX_BLACK;
    const uint16_t fg = sel ? ST77XX_BLACK : ST77XX_WHITE;
    if (sel) g.fillRect(0, y - 2, g.width(), 18, bg);

    char line[24];
    snprintf(line, sizeof(line), "%s %s", labels[i], values[i]);
    put(g, 4, y, fg, bg, line);
  }

  UiChrome::drawStatusBar(g, *_ctx);
  _ctx->display.endFrame();
  _menuDirty = false;
}

void ModuleManager::update() {
  if (!_ctx || !_entered) {
    if (_ctx && _count > 0 && !_entered) {
      setActive(0);
    }
    return;
  }

  publishUiMeta();
  handleButtons();

  if (_ctx->standby) return;

  if (_mode == UiMode::Menu) {
    if (_menuDirty) paintMenu();
    else {
      // refresh status bar chrome occasionally
      static int8_t lastFlash = -9;
      static bool lastWifi = false;
      static uint32_t lastWx = 0;
      static uint32_t lastBle = 0;
      static bool lastBleConn = false;
      const int8_t f = _ctx->activeFlashKey();
      const bool w = _ctx->net.wifiConnected();
      const uint32_t wx = _ctx->net.weather().fetchedAtMs;
      const uint32_t ble = _ctx->ble.lastCommitMs();
      const bool bleC = _ctx->ble.clientConnected();
      if (f != lastFlash || w != lastWifi || wx != lastWx || ble != lastBle ||
          bleC != lastBleConn) {
        lastFlash = f;
        lastWifi = w;
        lastWx = wx;
        lastBle = ble;
        lastBleConn = bleC;
        paintMenu();
      }
    }
    return;
  }

  if (_mode == UiMode::Settings) {
    if (_menuDirty) paintSettings();
    else {
      static int8_t lastFlashS = -9;
      static bool lastWifiS = false;
      static uint32_t lastWxS = 0;
      const int8_t f = _ctx->activeFlashKey();
      const bool w = _ctx->net.wifiConnected();
      const uint32_t wx = _ctx->net.weather().fetchedAtMs;
      if (f != lastFlashS || w != lastWifiS || wx != lastWxS) {
        lastFlashS = f;
        lastWifiS = w;
        lastWxS = wx;
        paintSettings();
      }
    }
    return;
  }

  IModule *mod = active();
  if (mod) mod->update(*_ctx);
}

void ModuleManager::handleButtons() {
  if (!_ctx) return;

  // Standby: any key wakes (short or long)
  if (_ctx->standby) {
    for (uint8_t i = 0; i < 4; i++) {
      if (_ctx->buttons.wasPressed(i) || _ctx->buttons.wasLongPressed(i)) {
        _ctx->notifyKey(i);
        wakeFromStandby();
        return;
      }
    }
    return;
  }

  // Global: hold K4 3s → standby (any UI mode)
  if (_ctx->buttons.wasLongPressed(3)) {
    _ctx->notifyKey(3);
    _ctx->enterStandby();
    return;
  }

  // ---- Menu ----
  if (_mode == UiMode::Menu) {
    if (_ctx->buttons.wasPressed(1)) {  // K2 back → leave menu
      _ctx->notifyKey(1);
      closeMenu();
      return;
    }
    if (_ctx->buttons.wasPressed(2)) {  // K3 down
      _ctx->notifyKey(2);
      const uint8_t maxSel = _count;  // last = Settings
      _menuSel = (uint8_t)((_menuSel + 1) % (maxSel + 1));
      _menuDirty = true;
      return;
    }
    if (_ctx->buttons.wasPressed(3)) {  // K4 up
      _ctx->notifyKey(3);
      const uint8_t maxSel = _count;
      _menuSel = (uint8_t)((_menuSel + maxSel) % (maxSel + 1));
      _menuDirty = true;
      return;
    }
    if (_ctx->buttons.wasPressed(0)) {  // K1 confirm / enter
      _ctx->notifyKey(0);
      if (_menuSel < _count) {
        setActive(_menuSel);
      } else {
        openSettings();
      }
      return;
    }
    return;
  }

  // ---- Settings ----
  if (_mode == UiMode::Settings) {
    if (_ctx->buttons.wasPressed(1)) {  // K2 back to menu
      _ctx->notifyKey(1);
      openMenu();
      return;
    }
    if (_ctx->buttons.wasPressed(2)) {  // K3 down
      _ctx->notifyKey(2);
      _settingsSel = (uint8_t)((_settingsSel + 1) % 3);
      _menuDirty = true;
      return;
    }
    if (_ctx->buttons.wasPressed(3)) {  // K4 up
      _ctx->notifyKey(3);
      _settingsSel = (uint8_t)((_settingsSel + 2) % 3);
      _menuDirty = true;
      return;
    }
    if (_ctx->buttons.wasPressed(0)) {  // K1 toggle
      _ctx->notifyKey(0);
      switch (_settingsSel) {
        case 0: _ctx->settings.weatherAuto = !_ctx->settings.weatherAuto; break;
        case 1: _ctx->settings.imagesOn = !_ctx->settings.imagesOn; break;
        case 2: _ctx->settings.hypixelOn = !_ctx->settings.hypixelOn; break;
        default: break;
      }
      _ctx->settings.save();
      _menuDirty = true;
      return;
    }
    return;
  }

  // ---- Module mode ----
  // K4 = previous module, K3 = next module (hold K4 3s = standby)
  if (_ctx->buttons.wasPressed(3)) {
    _ctx->notifyKey(3);
    switchModule(-1);
    return;
  }
  if (_ctx->buttons.wasPressed(2)) {
    _ctx->notifyKey(2);
    switchModule(+1);
    return;
  }

  IModule *mod = active();
  if (!mod) return;

  // Clock (home): K2 opens menu; K1 refreshes weather. Else: K1/K2 page.
  const bool isHome = (mod->number() == 1);
  if (_ctx->buttons.wasPressed(0)) {
    _ctx->notifyKey(0);
    mod->onButton(*_ctx, 0, true);
    return;
  }
  if (_ctx->buttons.wasPressed(1)) {
    _ctx->notifyKey(1);
    if (isHome) openMenu();
    else mod->onButton(*_ctx, 1, true);
  }
}
