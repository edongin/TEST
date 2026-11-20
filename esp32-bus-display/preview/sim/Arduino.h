// PC 미리보기용 가짜 Arduino.h
// 화면 그리기 코드(display.cpp)와 Adafruit GFX 가 쓰는 만큼만 흉내 냅니다.
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

// Arduino 가 미리 정해 둔 이름들 (PC 테스트와 같은 것을 씁니다)
#include "../../test_native/arduino_names.h"

#define PROGMEM
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#define pgm_read_word(addr) (*(const uint16_t*)(addr))
#define pgm_read_dword(addr) (*(const uint32_t*)(addr))

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))

typedef bool boolean;
typedef uint8_t byte;

// Arduino 의 String 중에서 Adafruit GFX 가 쓰는 부분만
class String {
 public:
  String(const char* s = "") : s_(s ? s : "") {}
  const char* c_str() const { return s_.c_str(); }
  unsigned int length() const { return (unsigned int)s_.size(); }

 private:
  std::string s_;
};

uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void yield();
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);

#include "Print.h"
