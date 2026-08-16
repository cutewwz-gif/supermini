#pragma once

#include "IModule.h"
#include "AppContext.h"

class ImageModule : public IModule {
public:
  const char *id() const override { return "images"; }
  const char *title() const override { return "Images"; }
  int8_t number() const override { return 0; }

  void onEnter(AppContext &ctx) override;
  void onExit(AppContext &ctx) override;
  void update(AppContext &ctx) override;
  void onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) override;

private:
  static constexpr uint8_t kMaxImages = 16;
  static constexpr int16_t kImgW = 160;
  static constexpr int16_t kImgH = 116;
  static constexpr size_t kImgBytes = (size_t)kImgW * (size_t)kImgH * 2;

  void paint(AppContext &ctx);
  void ensureList(AppContext &ctx, bool force);
  bool loadCurrent(AppContext &ctx);
  void freeBuffer();
  void syncChrome(AppContext &ctx);

  char _ids[kMaxImages][16];
  char _loadedId[16] = {};
  uint8_t _count = 0;
  uint8_t _index = 0;
  bool _listOk = false;
  bool _imgOk = false;
  bool _dirty = true;
  bool _busy = false;
  bool _wantList = true;
  bool _wantImage = false;
  uint32_t _lastFetchMs = 0;
  int8_t _lastFlash = -2;
  bool _lastWifi = false;
  uint16_t *_pixels = nullptr;
};
