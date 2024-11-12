#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <AnimatedGIF.h>
#include <Arduino.h>
#include <LittleFS.h>
#include <SPI.h>
#include <SimpleTouch.h>
#include <StreamUtils.h>
#include <gamma.h>

// #define DEBUG

#define HALT()                                                                 \
  {                                                                            \
    analogWrite(TFT_BL, 255);                                                  \
    digitalWrite(LED_BUILTIN, HIGH);                                           \
    while (true) {                                                             \
      yield();                                                                 \
    }                                                                          \
  }

const uint8_t TFT_CS = 5;
const uint8_t TFT_RST = 16;
const uint8_t TFT_DC = 17;
const uint8_t TFT_BL = 32;

const gpio_num_t TOUCH_PIN = GPIO_NUM_33;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
AnimatedGIF gif;
File file;
uint16_t* framebuf = nullptr;

SimpleTouch touch;

void* GIFOpenFile(const char* fname, int32_t* pSize) {
  file = LittleFS.open(fname);
  if (file) {
    *pSize = file.size();
    return (void*)&file;
  }
  return nullptr;
}

void GIFCloseFile(void* pHandle) {
  File* f = static_cast<File*>(pHandle);
  if (f != nullptr) {
    f->close();
    delete f;
  }
}

int32_t GIFReadFile(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen) {
  int32_t iBytesRead;
  iBytesRead = iLen;
  File* f = static_cast<File*>(pFile->fHandle);
  // Note: If you read a file all the way to the last byte, seek() stops working
  if ((pFile->iSize - pFile->iPos) < iLen) {
    iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
  }
  if (iBytesRead <= 0) {
    return 0;
  }
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}

int32_t GIFSeekFile(GIFFILE* pFile, int32_t iPosition) {
  File* f = static_cast<File*>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}

void GIFDraw(GIFDRAW* pDraw) {
  uint8_t* s;
  uint16_t *d, *usPalette;
  int x, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > tft.width())
    iWidth = tft.width() - pDraw->iX;
  usPalette = pDraw->pPalette;
  if (pDraw->iY + pDraw->y >= tft.height() || pDraw->iX >= tft.width() ||
      iWidth < 1)
    return;
  if (pDraw->y == 0) { // start of frame, set address window on LCD
    tft.dmaWait();     // wait for previous writes to complete before trying to
                       // access the LCD
    tft.setAddrWindow(pDraw->iX, pDraw->iY, pDraw->iWidth, pDraw->iHeight);
    // By setting the address window to the size of the current GIF frame, we
    // can just write continuously over the whole frame without having to set
    // the address window again
  }
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
  d = &framebuf[pDraw->iWidth * pDraw->y];
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t c, ucTransparent = pDraw->ucTransparent;
    int x;
    for (x = 0; x < iWidth; x++) {
      c = *s++;
      if (c != ucTransparent)
        d[x] = usPalette[c];
    }
  } else {
    // Translate the 8-bit pixels through the RGB565 palette (already byte
    // reversed)
    for (x = 0; x < iWidth; x++) {
      d[x] = usPalette[s[x]];
    }
  }
  tft.dmaWait(); // wait for last write to complete (the last scan line)
  // We write with block set to FALSE (3rd param) so that we can be decoding the
  // next line while the DMA hardware continues to write data to the LCD
  // controller
  tft.writePixels(d, iWidth, false, false);
}

uint8_t targetBL = 255;
const uint32_t MS_PER_PERCENT_CHANGE = 5;

void updateBacklight(bool instant = false) {
  static uint8_t currentBL = 255;
  static uint32_t timeSinceLastBLUpdate = 0;
  if (currentBL != targetBL || instant) {
    if (instant) {
      currentBL = targetBL;
    } else {
      // idk sometimes the brightness changes to zero randomly
      const uint32_t currentTime = millis();
      const float percentChange =
        ((float)currentTime - (float)timeSinceLastBLUpdate) /
        (float)MS_PER_PERCENT_CHANGE;
      currentBL +=
        constrain((int16_t)round(((float)targetBL - (float)currentBL) *
                                 (percentChange / 100)),
                  0, 255);
    }
    analogWrite(TFT_BL, gamma8[currentBL]);
  }
  timeSinceLastBLUpdate = millis();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  touch.begin(TOUCH_PIN);

  tft.initR(INITR_BLACKTAB);
  tft.setSPISpeed(40000000);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  pinMode(TFT_BL, OUTPUT);
  targetBL = 255;
  updateBacklight(true);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.setTextWrap(true);
  WriteLoggingStream logger(Serial, tft);

  if (!LittleFS.begin()) {
    logger.println("Failed to mount file system");
    HALT();
  }

  const uint16_t framebufSize = tft.width() * tft.height() * sizeof(uint16_t);
  framebuf = (uint16_t*)malloc(framebufSize);
  if (framebuf == nullptr) {
    logger.printf("Failed to allocate framebuffer of size %d bytes\n",
                  framebufSize);
    HALT();
  } else {
    memset(framebuf, 0, framebufSize);
    logger.printf("Allocated framebuffer of size %d bytes\n", framebufSize);
  }

  gif.begin(LITTLE_ENDIAN_PIXELS);

  logger.println("Opening GIF file /fireplace.gif");
  if (gif.open("/fireplace.gif", GIFOpenFile, GIFCloseFile, GIFReadFile,
               GIFSeekFile, GIFDraw)) {
    GIFINFO gi;
    logger.printf("Successfully opened GIF\nCanvas size is %d by %d px\n",
                  gif.getCanvasWidth(), gif.getCanvasHeight());
    if (gif.getInfo(&gi)) {
      logger.printf("frame count: %d\n", gi.iFrameCount);
      logger.printf("duration: %d ms\n", gi.iDuration);
      logger.printf("max delay: %d ms\n", gi.iMaxDelay);
      logger.printf("min delay: %d ms\n", gi.iMinDelay);
    } else {
      logger.println("Failed to get GIF info");
      HALT();
    }
#ifdef DEBUG
    delay(1000);
#endif
    while (true) {
      uint8_t i = 0;
      uint32_t frameStart = millis();
      while (true) {
        int toDelay;
        tft.startWrite();
        int result = gif.playFrame(false, &toDelay);
        tft.endWrite();
#ifdef DEBUG
        uint32_t frameDecodeEnd = millis();
#endif
        if (result == 0) {
          if (gif.getLastError() != GIF_SUCCESS &&
              gif.getLastError() != GIF_EMPTY_FRAME) {
            logger.printf("Error playing last frame = %d\n",
                          gif.getLastError());
            HALT();
          }
          break;
        } else if (result < 0) {
          logger.printf("Error playing frame = %d\n", gif.getLastError());
          HALT();
        }
#ifdef DEBUG
        tft.setCursor(0, 0);
        tft.printf("#%d %d ms    ", i++, frameDecodeEnd - frameStart);
#endif
        updateBacklight();
        while (millis() - frameStart < toDelay) {
          yield();
          touch.update();
          if (touch.justPressed()) {
            targetBL = targetBL == 255 ? 0 : 255;
          }
          digitalWrite(LED_BUILTIN, touch.isTouched());
        }
        frameStart = millis();
      }
      gif.reset();
    }
  } else {
    logger.printf("Error opening file = %d\n", gif.getLastError());
    HALT();
  }
}

void loop() {}
