/*
 * ESP32-C3 SuperMini OLED hello
 * Scans common I2C pin pairs, then shows text on SSD1306 if found.
 */

#include <Wire.h>
#include <U8g2lib.h>

struct PinPair {
  int sda;
  int scl;
};

static const PinPair kCandidates[] = {
  {5, 6},   // onboard 0.42" OLED SuperMini
  {8, 9},   // Arduino ESP32-C3 default Wire pins
  {6, 7},
  {4, 5},
  {1, 0},
  {3, 2},
  {10, 7},
  {20, 21},
};

static const uint8_t kOledAddrs[] = {0x3C, 0x3D};

int gSda = -1;
int gScl = -1;
uint8_t gAddr = 0;
bool gIs72x40 = false;

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2_72(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_64(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

bool i2cProbe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool findOled() {
  for (size_t i = 0; i < sizeof(kCandidates) / sizeof(kCandidates[0]); i++) {
    int sda = kCandidates[i].sda;
    int scl = kCandidates[i].scl;

    Serial.printf("Scan I2C SDA=%d SCL=%d ...\n", sda, scl);
    Wire.end();
    delay(20);
    Wire.begin(sda, scl);
    Wire.setClock(100000);
    delay(50);

    for (uint8_t addr : kOledAddrs) {
      if (i2cProbe(addr)) {
        gSda = sda;
        gScl = scl;
        gAddr = addr;
        Serial.printf("Found device at 0x%02X on SDA=%d SCL=%d\n", addr, sda, scl);
        return true;
      }
    }
  }
  return false;
}

bool initDisplay() {
  // Prefer 72x40 for common onboard OLED; fall back to 128x64.
  Wire.begin(gSda, gScl);
  Wire.setClock(400000);

  u8g2_72.setI2CAddress(gAddr << 1);
  if (u8g2_72.begin()) {
    gIs72x40 = true;
    Serial.println("Init OK: SSD1306 72x40");
    return true;
  }

  u8g2_64.setI2CAddress(gAddr << 1);
  if (u8g2_64.begin()) {
    gIs72x40 = false;
    Serial.println("Init OK: SSD1306 128x64");
    return true;
  }

  Serial.println("OLED init failed");
  return false;
}

void drawHello(uint32_t uptimeSec) {
  char line[32];
  snprintf(line, sizeof(line), "up %lus", (unsigned long)uptimeSec);

  if (gIs72x40) {
    u8g2_72.clearBuffer();
    u8g2_72.setFont(u8g2_font_6x10_tf);
    u8g2_72.drawStr(0, 10, "Hello!");
    u8g2_72.drawStr(0, 22, "ESP32-C3");
    u8g2_72.drawStr(0, 34, line);
    u8g2_72.sendBuffer();
  } else {
    u8g2_64.clearBuffer();
    u8g2_64.setFont(u8g2_font_ncenB14_tr);
    u8g2_64.drawStr(0, 18, "Hello!");
    u8g2_64.setFont(u8g2_font_6x10_tf);
    u8g2_64.drawStr(0, 36, "ESP32-C3 SuperMini");
    u8g2_64.drawStr(0, 50, line);
    u8g2_64.sendBuffer();
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("ESP32-C3 OLED hello");

  pinMode(8, OUTPUT);
  digitalWrite(8, LOW);  // onboard LED on (active low)

  if (!findOled()) {
    Serial.println("No OLED found on common I2C pins.");
    Serial.println("Check wiring: VCC=3V3, GND, SDA, SCL");
    return;
  }

  if (!initDisplay()) {
    return;
  }

  drawHello(0);
  digitalWrite(8, HIGH);
  Serial.println("Display ready");
}

void loop() {
  if (gAddr == 0) {
    digitalWrite(8, (millis() / 250) % 2 ? LOW : HIGH);
    delay(50);
    return;
  }

  drawHello(millis() / 1000);
  delay(1000);
}
