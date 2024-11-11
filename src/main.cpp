#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <AnimatedGIF.h>
#include <Arduino.h>
#include <SPI.h>
#include <badgers.h>

const uint8_t TFT_CS = 5;
const uint8_t TFT_RST = 16;
const uint8_t TFT_DC = 17;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
AnimatedGIF gif;

void GIFDraw(GIFDRAW* pDraw) {
  uint8_t* s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > tft.width())
    iWidth = tft.width() - pDraw->iX;
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line
  if (y >= tft.height() || pDraw->iX >= tft.width() || iWidth < 1)
    return;
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < iWidth) {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        } else // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      } // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        tft.startWrite();
        tft.setAddrWindow(pDraw->iX + x, y, iCount, 1);
        tft.writePixels(usTemp, iCount, false, false);
        tft.endWrite();
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount) {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  } else {
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte
    // reversed)
    for (x = 0; x < iWidth; x++)
      usTemp[x] = usPalette[*s++];
    tft.startWrite();
    tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
    tft.writePixels(usTemp, iWidth, false, false);
    tft.endWrite();
  }
} /* GIFDraw() */

void setup() {
  Serial.begin(115200);

  tft.initR(INITR_BLACKTAB);
  tft.setSPISpeed(40000000);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  gif.begin(LITTLE_ENDIAN_PIXELS);
}

void loop() {
  if (gif.open((uint8_t*)badgers, sizeof(badgers), GIFDraw)) {
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n",
                  gif.getCanvasWidth(), gif.getCanvasHeight());
    while (gif.playFrame(true, NULL)) {
    }
    gif.close();
  }
}
