// PC 미리보기용 가짜 Print (Adafruit_GFX 가 이것을 상속합니다)
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* buf, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buf++);
    return n;
  }
  size_t write(const char* s) { return s ? write((const uint8_t*)s, strlen(s)) : 0; }
  size_t print(const char* s) { return write(s); }
  size_t println(const char* s = "") { return write(s) + write((uint8_t)'\n'); }
};
