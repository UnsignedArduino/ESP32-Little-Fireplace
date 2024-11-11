#ifndef ESP32_PAGE_FLIPPER_TOUCHPAD_H
#define ESP32_PAGE_FLIPPER_TOUCHPAD_H

#include <Arduino.h>

#ifndef SIMPLE_TOUCH_CALIBRATION_READINGS
  #define SIMPLE_TOUCH_CALIBRATION_READINGS 10
#endif
#ifndef SIMPLE_TOUCH_CALIBRATION_READING_PERIOD
  #define SIMPLE_TOUCH_CALIBRATION_READING_PERIOD 20
#endif

class SimpleTouch {
  public:
    SimpleTouch() = default;
    ~SimpleTouch() = default;

    void begin(gpio_num_t pad);

    void calibrate();
    void update();

    bool isTouched() const;
    bool justPressed();
    bool justReleased();
    bool justToggled();

    void end();

    float threshold = 0.1;
    uint32_t debounceDelay = 50;

  private:
    gpio_num_t pad;

    bool thisState = false;
    bool justChanged = false;

    touch_value_t normal = 0;
    touch_value_t triggerUnder = 0;

    uint32_t ignoreUntil = 0;

    void setJustChangedFlag();
    bool readAndResetJustChangedFlag();
};

#endif // ESP32_PAGE_FLIPPER_TOUCHPAD_H
