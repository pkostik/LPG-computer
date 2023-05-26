#pragma once
#include <Arduino.h>
#define _BTN_DEB_TIME 100
class Button {
  public:
    Button (byte pin) : _pin(pin) {
      pinMode(_pin, INPUT_PULLUP);
    }
    bool click() {
      bool btnState = digitalRead(_pin);
      if (!btnState && !_flag && millis() - _tmr >= _BTN_DEB_TIME) {
        _flag = true;
        _tmr = millis();
        return true;
      }
      if (btnState && _flag && millis() - _tmr >= _BTN_DEB_TIME) {
        _flag = false;
        _tmr = millis();
      }
      return false;
    }
  private:
    const byte _pin;
    uint32_t _tmr;
    bool _flag;
};