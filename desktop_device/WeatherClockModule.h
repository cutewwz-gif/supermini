#pragma once

#include "IModule.h"
#include "AppContext.h"

class WeatherClockModule : public IModule {
public:
  const char *id() const override { return "weather_clock"; }
  const char *title() const override { return "Clock"; }
  int8_t number() const override { return 1; }

  void onEnter(AppContext &ctx) override;
  void update(AppContext &ctx) override;
  void onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) override;

private:
  void paint(AppContext &ctx);
  void syncSnapshot(AppContext &ctx);
  void paintTimeOnly(AppContext &ctx);

  uint32_t _lastUiMs = 0;
  int _lastSec = -1;
  int _lastYday = -1;
  bool _lastWifi = false;
  bool _lastWeatherValid = false;
  float _lastTemp = NAN;
  float _lastHum = NAN;
  int _lastCode = -1;
  bool _lastTipUmbrella = false;
  bool _lastTipHat = false;
  uint32_t _lastFetchMs = 0;
  uint32_t _lastBleMs = 0;
  bool _lastBleConn = false;
  int8_t _lastFlash = -2;
};
