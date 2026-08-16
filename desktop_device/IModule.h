#pragma once

#include <Arduino.h>

class AppContext;

// Every feature screen implements IModule.
// Add new modules by subclassing and registering in desktop_device.ino.
class IModule {
public:
  virtual ~IModule() {}
  virtual const char *id() const = 0;
  virtual const char *title() const = 0;
  // Signed module number shown in status bar (e.g. -1, 0, 1, 2).
  virtual int8_t number() const = 0;

  virtual void onEnter(AppContext &ctx) = 0;
  virtual void onExit(AppContext &ctx) {}
  virtual void update(AppContext &ctx) = 0;

  // btnIndex: 0=K1 .. 3=K4, pressed=true on falling edge
  virtual void onButton(AppContext &ctx, uint8_t btnIndex, bool pressed) {
    (void)ctx;
    (void)btnIndex;
    (void)pressed;
  }
};
