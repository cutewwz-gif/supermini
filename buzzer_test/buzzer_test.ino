/*
 * Buzzer bring-up for ESP32-C3 SuperMini
 * Wiring (two-pin, "正" marked):
 *   正  -> 3.3V
 *   other -> GPIO4
 * Also tries alternate drive if silent.
 */

#include <Arduino.h>

static const int PIN_BUZZ = 4;

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("buzzer_test boot");
  pinMode(PIN_BUZZ, OUTPUT);
  digitalWrite(PIN_BUZZ, HIGH);
}

void beepActiveLow(uint32_t ms) {
  // Sink mode: 正=3V3, GPIO low => current flows
  digitalWrite(PIN_BUZZ, LOW);
  delay(ms);
  digitalWrite(PIN_BUZZ, HIGH);
}

void beepActiveHigh(uint32_t ms) {
  digitalWrite(PIN_BUZZ, HIGH);
  delay(ms);
  digitalWrite(PIN_BUZZ, LOW);
}

void beepPassive(uint32_t freq, uint32_t ms) {
  // Passive buzzers need a square wave
  tone(PIN_BUZZ, freq, ms);
  delay(ms + 30);
  noTone(PIN_BUZZ);
}

void loop() {
  Serial.println("--- active LOW (recommended if 正->3V3) ---");
  for (int i = 0; i < 3; i++) {
    beepActiveLow(200);
    delay(200);
  }
  delay(600);

  Serial.println("--- active HIGH ---");
  for (int i = 0; i < 3; i++) {
    beepActiveHigh(200);
    delay(200);
  }
  delay(600);

  Serial.println("--- passive PWM 2k/3k/4k ---");
  beepPassive(2000, 300);
  delay(150);
  beepPassive(3000, 300);
  delay(150);
  beepPassive(4000, 300);
  delay(1200);
}
