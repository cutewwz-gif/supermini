/*
 * Modular dorm desktop — ESP32-C3 SuperMini + 1.8" ST7735
 *
 * BEFORE CHANGING UI: read UI_CONTRACT.md
 *
 * Modules:
 *   #-1 Help      ReadmeModule
 *   #0  Images    ImageModule
 *   #1  Clock     WeatherClockModule
 *   #2  Forecast  WeatherForecastModule
 *   #3  Hypixel   HypixelModule
 */

#include "config.h"
#include "AppContext.h"
#include "ModuleManager.h"
#include "ReadmeModule.h"
#include "ImageModule.h"
#include "WeatherClockModule.h"
#include "WeatherForecastModule.h"
#include "HypixelModule.h"

AppContext app;
ModuleManager modules;
ReadmeModule readme;
ImageModule images;
WeatherClockModule weatherClock;
WeatherForecastModule weatherForecast;
HypixelModule hypixel;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("desktop_device boot");

  app.begin();

  modules.begin(&app);
  modules.registerModule(&readme);          // #-1 Help
  modules.registerModule(&images);          // #0 Images
  modules.registerModule(&weatherClock);    // #1 Clock
  modules.registerModule(&weatherForecast); // #2 Forecast
  modules.registerModule(&hypixel);         // #3 Hypixel
  modules.setActive(2);  // boot into Clock (#1)

  Serial.println("modules ready");
}

void loop() {
  app.update();
  modules.update();
}
