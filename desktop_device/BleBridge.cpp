#include "BleBridge.h"
#include "NetServices.h"
#include "config.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <string.h>
#include <stdlib.h>

// Custom 128-bit UUIDs (must match watch_proxy)
static const char *kSvcUuid = "6bc80001-a1b2-c3d4-e5f6-000000000001";
static const char *kDataUuid = "6bc80002-a1b2-c3d4-e5f6-000000000001";
static const char *kCtrlUuid = "6bc80003-a1b2-c3d4-e5f6-000000000001";

namespace {

BleBridge *g_ble = nullptr;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *s) override {
    (void)s;
    if (g_ble) g_ble->onConnect();
  }
  void onDisconnect(BLEServer *s) override {
    (void)s;
    if (g_ble) g_ble->onDisconnect();
  }
};

class DataCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    if (!g_ble) return;
    String v = c->getValue();
    if (v.length() == 0) return;
    g_ble->onDataWrite(reinterpret_cast<const uint8_t *>(v.c_str()), (size_t)v.length());
  }
};

class CtrlCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    if (!g_ble) return;
    String v = c->getValue();
    if (v.length() == 0) return;
    g_ble->onCtrlWrite(reinterpret_cast<const uint8_t *>(v.c_str()), (size_t)v.length());
  }
};

}  // namespace

void BleBridge::begin(NetServices *net) {
  _net = net;
  g_ble = this;
  _buf = (char *)malloc(kBufMax);
  if (!_buf) {
    Serial.println("[ble] buffer alloc failed");
    return;
  }
  resetBuffer();

  BLEDevice::init("SuperMini");
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *svc = server->createService(kSvcUuid);
  BLECharacteristic *data =
      svc->createCharacteristic(kDataUuid, BLECharacteristic::PROPERTY_WRITE |
                                               BLECharacteristic::PROPERTY_WRITE_NR);
  data->setCallbacks(new DataCallbacks());

  BLECharacteristic *ctrl =
      svc->createCharacteristic(kCtrlUuid, BLECharacteristic::PROPERTY_WRITE |
                                               BLECharacteristic::PROPERTY_WRITE_NR);
  ctrl->setCallbacks(new CtrlCallbacks());

  svc->start();
  _started = true;
  setEnabled(true);
  Serial.println("[ble] SuperMini advertising (watch proxy)");
}

void BleBridge::setEnabled(bool on) {
  if (!_started) return;
  if (on == _enabled) return;
  _enabled = on;
  if (on) startAdvertising();
  else stopAdvertising();
}

void BleBridge::startAdvertising() {
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(kSvcUuid);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
}

void BleBridge::stopAdvertising() {
  BLEDevice::stopAdvertising();
}

void BleBridge::resetBuffer() {
  _len = 0;
  if (_buf) _buf[0] = '\0';
}

void BleBridge::onConnect() {
  _connected = true;
  Serial.println("[ble] watch connected");
}

void BleBridge::onDisconnect() {
  _connected = false;
  Serial.println("[ble] watch disconnected");
  if (_enabled) startAdvertising();
}

void BleBridge::onDataWrite(const uint8_t *data, size_t len) {
  if (!_buf || !data || len == 0) return;
  if (_len + len >= kBufMax) {
    Serial.println("[ble] buffer overflow — send CLEAR");
    resetBuffer();
    return;
  }
  memcpy(_buf + _len, data, len);
  _len += len;
  _buf[_len] = '\0';
}

void BleBridge::onCtrlWrite(const uint8_t *data, size_t len) {
  if (!data || len == 0) return;
  // Accept "CLEAR" / "COMMIT" (ignore trailing junk)
  char cmd[16];
  size_t n = len < sizeof(cmd) - 1 ? len : sizeof(cmd) - 1;
  memcpy(cmd, data, n);
  cmd[n] = '\0';
  for (size_t i = 0; i < n; i++) {
    if (cmd[i] == '\r' || cmd[i] == '\n' || cmd[i] == ' ') {
      cmd[i] = '\0';
      break;
    }
  }

  if (strcmp(cmd, "CLEAR") == 0) {
    resetBuffer();
    Serial.println("[ble] CLEAR");
    return;
  }
  if (strcmp(cmd, "COMMIT") == 0) {
    _commitPending = true;
    return;
  }
  Serial.printf("[ble] ctrl ? %s\n", cmd);
}

void BleBridge::tryCommit() {
  if (!_commitPending) return;
  _commitPending = false;
  if (!_net || _len == 0) {
    Serial.println("[ble] COMMIT empty");
    resetBuffer();
    return;
  }
  Serial.printf("[ble] COMMIT %u bytes\n", (unsigned)_len);
  const bool ok = _net->applyWeatherJson(_buf, _len);
  _lastCommitOk = ok;
  _lastCommitMs = millis();
  Serial.printf("[ble] apply %s\n", ok ? "ok" : "fail");
  resetBuffer();
}

void BleBridge::update() {
  tryCommit();
}
