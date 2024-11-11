#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Arduino.h>
#include <SPI.h>

const uint8_t TFT_CS = 5;
const uint8_t TFT_RST = 16;
const uint8_t TFT_DC = 17;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);

  tft.initR(INITR_BLACKTAB);
  tft.setSPISpeed(40000000);

  tft.fillScreen(ST77XX_BLACK);
}

void loop() {}
