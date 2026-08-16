#include "NetServices.h"
#include "WeatherCodes.h"
#include "config.h"
#include "secrets.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

void NetServices::begin() {
  WiFi.mode(WIFI_STA);
  // Allow modem sleep while associated — big win when not transferring.
  WiFi.setSleep(true);
  _wifiOk = connectWifi();
  if (_wifiOk) {
    _timeOk = syncTime();
  }
  // Weather/forecast: manual or on-demand from modules only (no timed polling).
  _weatherWanted = false;
}

void NetServices::update() {
  _wifiOk = (WiFi.status() == WL_CONNECTED);
  if (!_wifiOk) {
    static uint32_t lastReconnect = 0;
    if (millis() - lastReconnect > 15000) {
      lastReconnect = millis();
      _wifiOk = connectWifi();
      if (_wifiOk && !_timeOk) {
        _timeOk = syncTime();
      }
    }
    return;
  }

  if (!_timeOk) {
    _timeOk = syncTime();
  }

  checkWeatherSchedule();

  // Weather: manual / schedule request, and/or Auto 10-minute refresh
  const uint32_t now = millis();
  const bool stale = !_weather.valid ||
                     (now - _weather.fetchedAtMs) > WEATHER_REFRESH_MS;
  if ((_weatherWanted || (_weatherAuto && stale)) &&
      (now - _lastWeatherAttemptMs) > 3000) {
    _lastWeatherAttemptMs = now;
    _weatherWanted = false;
    fetchWeather();
  }
}

void NetServices::checkWeatherSchedule() {
  // Fixed daily slots (always): 05:45, 08:00, 10:00, 21:00, 23:00
  static const uint8_t kSlots[][2] = {
      {5, 45}, {8, 0}, {10, 0}, {21, 0}, {23, 0}};
  struct tm tinfo = {};
  if (!getLocalTime(&tinfo, 0)) return;

  for (uint8_t i = 0; i < sizeof(kSlots) / sizeof(kSlots[0]); i++) {
    if ((uint8_t)tinfo.tm_hour == kSlots[i][0] &&
        (uint8_t)tinfo.tm_min == kSlots[i][1]) {
      const int32_t key = (int32_t)tinfo.tm_yday * 10 + (int32_t)i;
      if (_lastSchedKey != key) {
        _lastSchedKey = key;
        requestWeatherRefresh();
      }
      return;
    }
  }
}

bool NetServices::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool NetServices::timeSynced() const {
  return _timeOk;
}

void NetServices::requestWeatherRefresh() {
  _weatherWanted = true;
  _lastWeatherAttemptMs = 0;
}

bool NetServices::connectWifi() {
  if (String(WIFI_SSID) == "YOUR_WIFI_SSID" || WIFI_SSID[0] == '\0') {
    Serial.println("[wifi] secrets.h not configured");
    return false;
  }

  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.printf("[wifi] connecting to %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(200);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] ok %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println("[wifi] failed");
  return false;
}

bool NetServices::syncTime() {
  configTzTime(TZ_INFO, "ntp.aliyun.com", "ntp.tencent.com", "pool.ntp.org");
  struct tm tinfo;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&tinfo, 500)) {
      Serial.printf("[time] synced %04d-%02d-%02d %02d:%02d:%02d\n",
                    tinfo.tm_year + 1900, tinfo.tm_mon + 1, tinfo.tm_mday,
                    tinfo.tm_hour, tinfo.tm_min, tinfo.tm_sec);
      return true;
    }
  }
  Serial.println("[time] sync failed");
  return false;
}

bool NetServices::isNightAt(time_t when) const {
  if (when <= 0) return false;
  if (!_astro.valid || _astro.dayCount == 0) {
    struct tm *tp = localtime(&when);
    if (!tp) return false;
    return (tp->tm_hour >= 19 || tp->tm_hour < 6);
  }
  for (uint8_t i = 0; i < _astro.dayCount; i++) {
    const DayAstro &d = _astro.days[i];
    if (when < d.riseEpoch) return true;
    if (when >= d.riseEpoch && when < d.setEpoch) return false;
  }
  return true;
}

bool NetServices::isNightNow() const {
  struct tm nowTm = {};
  if (!getLocalTime(&nowTm, 0)) return false;
  return isNightAt(mktime(&nowTm));
}

void NetServices::rebuildAstroCaches() {
  _astro.nextSunriseValid = false;
  struct tm nowTm = {};
  if (!getLocalTime(&nowTm, 0) || !_astro.valid) return;
  const time_t nowEpoch = mktime(&nowTm);

  for (uint8_t i = 0; i < _astro.dayCount; i++) {
    const DayAstro &d = _astro.days[i];
    if (d.riseEpoch > nowEpoch) {
      _astro.nextSunriseValid = true;
      _astro.nextMo = d.month;
      _astro.nextDy = d.day;
      _astro.nextH = d.riseH;
      _astro.nextM = d.riseM;
      return;
    }
  }
}

bool NetServices::fetchWeather() {
  if (!wifiConnected()) return false;

  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(SUPERMINI_API_WEATHER)) {
    Serial.println("[wx] begin failed");
    return false;
  }
  http.addHeader("X-SuperMini-Key", SUPERMINI_API_KEY);

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[wx] HTTP %d\n", code);
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();
  return applyWeatherJson(body.c_str(), body.length());
}

bool NetServices::applyWeatherJson(const char *json, size_t len) {
  if (!json || len == 0) return false;

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, json, len);
  if (err) {
    Serial.printf("[wx] json %s\n", err.c_str());
    return false;
  }
  if (!(doc["ok"] | false)) {
    Serial.printf("[wx] api err %s\n", doc["error"] | "?");
    return false;
  }

  JsonObject cur = doc["current"].as<JsonObject>();
  if (!cur.isNull()) {
    _weather.temperatureC = cur["tempC"] | NAN;
    _weather.humidity = cur["humidity"] | NAN;
    _weather.windMs = cur["windMs"] | NAN;
    _weather.weatherCode = cur["code"] | -1;
    const char *sum = cur["summary"] | "";
    if (sum && sum[0]) {
      strncpy(_weather.summary, sum, sizeof(_weather.summary) - 1);
      _weather.summary[sizeof(_weather.summary) - 1] = '\0';
    } else {
      weatherSummaryLong(_weather.weatherCode, _weather.summary, sizeof(_weather.summary));
    }
    _weather.fetchedAtMs = millis();
    _weather.valid = true;
    // Stamp wall time: watch syncH/M (BLE) → hub fetched_at → local NTP
    _weather.fetchedTimeValid = false;
    if (!doc["syncH"].isNull() && !doc["syncM"].isNull()) {
      _weather.fetchedHour = (uint8_t)(doc["syncH"] | 0);
      _weather.fetchedMin = (uint8_t)(doc["syncM"] | 0);
      _weather.fetchedTimeValid = true;
    } else if (!doc["fetched_at"].isNull()) {
      const time_t ep = (time_t)(doc["fetched_at"] | 0);
      if (ep > 0) {
        struct tm *tp = localtime(&ep);
        if (tp) {
          _weather.fetchedHour = (uint8_t)tp->tm_hour;
          _weather.fetchedMin = (uint8_t)tp->tm_min;
          _weather.fetchedTimeValid = true;
        }
      }
    }
    if (!_weather.fetchedTimeValid) {
      struct tm fetchedTm = {};
      if (getLocalTime(&fetchedTm, 0)) {
        _weather.fetchedHour = (uint8_t)fetchedTm.tm_hour;
        _weather.fetchedMin = (uint8_t)fetchedTm.tm_min;
        _weather.fetchedTimeValid = true;
      }
    }
  }

  JsonObject tips = doc["tips"].as<JsonObject>();
  if (!tips.isNull()) {
    _weather.tipUmbrella = tips["umbrella"] | false;
    _weather.tipHat = tips["hat"] | false;
  } else {
    _weather.tipUmbrella = false;
    _weather.tipHat = false;
  }

  ForecastSnapshot fc = {};
  AstroSnapshot as = {};

  JsonArray hourly = doc["hourly"].as<JsonArray>();
  if (!hourly.isNull()) {
    for (JsonObject h : hourly) {
      if (fc.hourlyCount >= kMaxHourlyForecast) break;
      HourlyForecastPoint &p = fc.hourly[fc.hourlyCount++];
      p.month = (uint8_t)(h["mo"] | 0);
      p.day = (uint8_t)(h["dy"] | 0);
      p.hour = (uint8_t)(h["h"] | 0);
      p.tempC = (int8_t)(h["t"] | 0);
      p.weatherCode = (int16_t)(h["c"] | -1);
      p.precipProb = (int8_t)(h["p"] | -1);
    }
  }

  JsonArray daily = doc["daily"].as<JsonArray>();
  if (!daily.isNull()) {
    for (JsonObject d : daily) {
      if (fc.dailyCount >= kMaxDailyForecast) break;
      DailyForecastPoint &p = fc.daily[fc.dailyCount++];
      p.month = (uint8_t)(d["mo"] | 0);
      p.day = (uint8_t)(d["dy"] | 0);
      p.tMin = (int8_t)(d["tMin"] | 0);
      p.tMax = (int8_t)(d["tMax"] | 0);
      p.weatherCode = (int16_t)(d["c"] | -1);
      p.precipProb = (int8_t)(d["p"] | -1);
    }
  }

  JsonArray astro = doc["astro"].as<JsonArray>();
  if (!astro.isNull()) {
    for (JsonObject a : astro) {
      if (as.dayCount >= 8) break;
      DayAstro &d = as.days[as.dayCount++];
      d.month = (uint8_t)(a["mo"] | 0);
      d.day = (uint8_t)(a["dy"] | 0);
      d.riseH = (uint8_t)(a["riseH"] | 0);
      d.riseM = (uint8_t)(a["riseM"] | 0);
      d.setH = (uint8_t)(a["setH"] | 0);
      d.setM = (uint8_t)(a["setM"] | 0);
      d.riseEpoch = (time_t)(a["riseEpoch"] | 0);
      d.setEpoch = (time_t)(a["setEpoch"] | 0);
    }
    as.valid = as.dayCount > 0;
  }

  fc.valid = (fc.hourlyCount > 0 || fc.dailyCount > 0);
  fc.fetchedAtMs = millis();
  if (fc.valid) _forecast = fc;
  if (as.valid) {
    _astro = as;
    rebuildAstroCaches();
  }

  Serial.printf("[wx] apply now=%.1fC hourly=%u daily=%u tips u=%d h=%d\n",
                _weather.temperatureC, _forecast.hourlyCount, _forecast.dailyCount,
                (int)_weather.tipUmbrella, (int)_weather.tipHat);
  return _weather.valid || _forecast.valid;
}
