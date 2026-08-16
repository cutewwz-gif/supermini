#pragma once

#include "IModule.h"
#include "AppContext.h"

class HypixelModule : public IModule {
public:
  const char *id() const override { return "hypixel"; }
  const char *title() const override { return "Hypixel"; }
  int8_t number() const override { return 3; }

  void onEnter(AppContext &ctx) override;
  void update(AppContext &ctx) override;
  void onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) override;

private:
  static constexpr uint8_t kMaxSeg = 8;

  struct Seg {
    char text[24];
    uint16_t color;
  };

  struct Snapshot {
    bool valid = false;
    uint32_t fetchedAtMs = 0;
    char activity[28] = "";
    bool online = false;
    Seg segs[kMaxSeg];
    uint8_t segCount = 0;
    int stars = 0;
    int kills = 0, deaths = 0;
    float kdr = 0;
    int fk = 0, fd = 0;
    float fkdr = 0;
    int wins = 0, losses = 0, beds = 0;
  };

  void paint(AppContext &ctx);
  void ensureData(AppContext &ctx, bool force);
  bool fetchFromServer();
  static uint16_t colorFromName(const char *name);
  void syncChrome(AppContext &ctx);

  Snapshot _snap;
  bool _dirty = true;
  bool _busy = false;
  bool _wantFetch = true;
  uint32_t _lastFetchMs = 0;
  int8_t _lastFlash = -2;
  bool _lastWifi = false;
};
