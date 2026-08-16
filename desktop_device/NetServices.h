#pragma once

#include <Arduino.h>
#include <math.h>
#include <time.h>

struct WeatherSnapshot {
  bool valid = false;
  float temperatureC = NAN;
  float humidity = NAN;
  float windMs = NAN;
  int weatherCode = -1;
  char summary[24] = "N/A";
  uint32_t fetchedAtMs = 0;
  // Wall-clock time of last successful fetch (for status bar)
  bool fetchedTimeValid = false;
  uint8_t fetchedHour = 0;
  uint8_t fetchedMin = 0;
  // Rest-of-today advice (from hub)
  bool tipUmbrella = false;
  bool tipHat = false;
};

struct HourlyForecastPoint {
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  int8_t tempC = 0;
  int16_t weatherCode = -1;
  int8_t precipProb = -1;
};

struct DailyForecastPoint {
  uint8_t month = 0;
  uint8_t day = 0;
  int8_t tMin = 0;
  int8_t tMax = 0;
  int16_t weatherCode = -1;
  int8_t precipProb = -1;
};

struct DayAstro {
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t riseH = 0;
  uint8_t riseM = 0;
  uint8_t setH = 0;
  uint8_t setM = 0;
  time_t riseEpoch = 0;
  time_t setEpoch = 0;
};

struct AstroSnapshot {
  bool valid = false;
  uint8_t dayCount = 0;
  DayAstro days[8];
  // Cached "next visible sunrise"
  bool nextSunriseValid = false;
  uint8_t nextMo = 0, nextDy = 0, nextH = 0, nextM = 0;
};

static const uint8_t kMaxHourlyForecast = 48;
static const uint8_t kMaxDailyForecast = 7;

struct ForecastSnapshot {
  bool valid = false;
  uint32_t fetchedAtMs = 0;
  uint8_t hourlyCount = 0;
  uint8_t dailyCount = 0;
  HourlyForecastPoint hourly[kMaxHourlyForecast];
  DailyForecastPoint daily[kMaxDailyForecast];
};

class NetServices {
public:
  void begin();
  void update();

  bool wifiConnected() const;
  bool timeSynced() const;
  const WeatherSnapshot &weather() const { return _weather; }
  const ForecastSnapshot &forecast() const { return _forecast; }
  const AstroSnapshot &astro() const { return _astro; }

  bool isNightNow() const;
  bool isNightAt(time_t when) const;
  void requestWeatherRefresh();
  void setWeatherAuto(bool enabled) { _weatherAuto = enabled; }
  // Apply compact hub JSON (WiFi fetch or BLE watch proxy).
  bool applyWeatherJson(const char *json, size_t len);

private:
  bool connectWifi();
  bool syncTime();
  bool fetchWeather();
  void rebuildAstroCaches();
  void checkWeatherSchedule();

  bool _wifiOk = false;
  bool _timeOk = false;
  bool _weatherWanted = false;
  bool _weatherAuto = false;
  uint32_t _lastWeatherAttemptMs = 0;
  // Last schedule slot fired: packed yday*10 + slotIndex, or -1
  int32_t _lastSchedKey = -1;
  WeatherSnapshot _weather;
  ForecastSnapshot _forecast;
  AstroSnapshot _astro;
};
