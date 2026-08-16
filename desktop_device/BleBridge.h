#pragma once

#include <Arduino.h>

class NetServices;

// BLE peripheral: watch opens app → writes weather JSON → COMMIT.
// Designed for foreground-only watch sync (no background daemon).
class BleBridge {
public:
  void begin(NetServices *net);
  void update();
  void setEnabled(bool on);
  bool enabled() const { return _enabled; }
  bool clientConnected() const { return _connected; }
  bool lastCommitOk() const { return _lastCommitOk; }
  uint32_t lastCommitMs() const { return _lastCommitMs; }

  // Called from BLE callbacks (keep short).
  void onDataWrite(const uint8_t *data, size_t len);
  void onCtrlWrite(const uint8_t *data, size_t len);
  void onConnect();
  void onDisconnect();

private:
  void startAdvertising();
  void stopAdvertising();
  void resetBuffer();
  void tryCommit();

  NetServices *_net = nullptr;
  bool _enabled = false;
  bool _started = false;
  bool _connected = false;
  bool _commitPending = false;
  bool _lastCommitOk = false;
  uint32_t _lastCommitMs = 0;

  static const size_t kBufMax = 5120;
  char *_buf = nullptr;
  size_t _len = 0;
};
