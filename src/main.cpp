#include <Arduino.h>

#include "ilda.h"
#include "SPIRenderer.h"
#include "network.h"
#include "button.h"
#include "SDCard.h"

int kppsTime = 1000000 / (20 * 1000);
volatile unsigned long timeOld;

void setup()
{
  Serial.begin(115200);
  
  setupWifi();
  setupWebServer();
  setupSD();
  setupILDA();
  setupRenderer();
  setupButton();
}

void loop()
{
  if (micros() - timeOld >= kppsTime)
  {
    timeOld = micros();
    draw_task();
  }
  buttonL.loop();
  buttonR.loop();
}
