#pragma once

#include "IModule.h"
#include "AppContext.h"

class ReadmeModule : public IModule {
public:
  const char *id() const override { return "readme"; }
  const char *title() const override { return "Help"; }
  int8_t number() const override { return -1; }

  void onEnter(AppContext &ctx) override;
  void update(AppContext &ctx) override;
  void onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) override;

private:
  static constexpr uint8_t kPages = 4;
  void paint(AppContext &ctx);
  void clampPage();

  uint8_t _page = 0;
  bool _dirty = true;
  int8_t _lastFlash = -2;
  bool _lastWifi = false;
};
