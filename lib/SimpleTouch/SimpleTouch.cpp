#include "SimpleTouch.h"

void SimpleTouch::begin(gpio_num_t pad) {
  this->pad = pad;
  this->calibrate();
}

void SimpleTouch::calibrate() {
  touch_value_t sum = 0;
  for (int i = 0; i < SIMPLE_TOUCH_CALIBRATION_READINGS; i++) {
    sum += touchRead(this->pad);
    delay(SIMPLE_TOUCH_CALIBRATION_READING_PERIOD);
  }
  this->normal = sum / SIMPLE_TOUCH_CALIBRATION_READINGS;
  this->triggerUnder = this->normal * (1 - this->threshold);
}

void SimpleTouch::update() {
  if (millis() < this->ignoreUntil) {
    return;
  }

  const bool currentState = touchRead(this->pad) < this->triggerUnder;

  if (this->thisState != currentState) {
    this->ignoreUntil = millis() + this->debounceDelay;
    this->thisState = currentState;
    this->setJustChangedFlag();
  }
}

bool SimpleTouch::isTouched() const {
  return this->thisState;
}

bool SimpleTouch::justPressed() {
  return this->thisState && this->readAndResetJustChangedFlag();
}

bool SimpleTouch::justReleased() {
  return !this->thisState && this->readAndResetJustChangedFlag();
}

bool SimpleTouch::justToggled() {
  return this->readAndResetJustChangedFlag();
}

void SimpleTouch::setJustChangedFlag() {
  this->justChanged = true;
}

bool SimpleTouch::readAndResetJustChangedFlag() {
  const bool result = this->justChanged;
  this->justChanged = false;
  return result;
}

void SimpleTouch::end() {}
