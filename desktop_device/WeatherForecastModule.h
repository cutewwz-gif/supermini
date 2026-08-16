#pragma once

#include "IModule.h"
#include "AppContext.h"

class WeatherForecastModule : public IModule {
public:
  const char *id() const override { return "weather_forecast"; }
  const char *title() const override { return "Forecast"; }
  int8_t number() const override { return 2; }

  void onEnter(AppContext &ctx) override;
  void update(AppContext &ctx) override;
  void onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) override;

private:
  void paint(AppContext &ctx);
  void paintHourly(AppContext &ctx, uint8_t localPage);
  void paintDailyNear(AppContext &ctx);  // page -1
  void paintDailyFar(AppContext &ctx);   // page -2
  uint8_t hourlyPages(AppContext &ctx) const;
  uint8_t dailyPages(AppContext &ctx) const;
  uint8_t totalPages(AppContext &ctx) const;
  void clampPage(AppContext &ctx);
  static bool hourIsNight(AppContext &ctx, uint8_t month, uint8_t day, uint8_t hour);

  uint8_t _page = 0;  // daily -2, -1, then hourly 1..N
  bool _preferNearDaily = true;  // open on -1 by default
  uint32_t _lastForecastMs = 0;
  bool _dirty = true;
  int8_t _lastFlash = -2;
  bool _lastWifi = false;
};
