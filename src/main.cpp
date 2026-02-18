#include "display.hpp"
#include "scale.hpp"
#include "otaUpdate.hpp"
#include "./menu/MenuController.hpp"

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(100);
  }

  // wifi and OTA init
  otaSetup();

  // Load persisted settings from NVS before hardware init reads them.
  // (Menu constructors run at static-init time before NVS is mounted.)
  loadAllMenuSettings();

  // Display should handle frontend only
  setupDisplay();

  // Scale should handle load cell functions only
  setupScale();

  // Menu handles backend settings and tracking state
  setupMenu();

  Serial.println();
  Serial.println("******************************************************");
}

void loop()
{
  otaServerLoop();
  delay(1000);
}
