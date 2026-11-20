// PC 미리보기용 가짜 SPI. 아무 일도 하지 않습니다.
#pragma once

#include <stdint.h>

class SPIClass {
 public:
  SPIClass() {}
  explicit SPIClass(int) {}
  void begin(int8_t = -1, int8_t = -1, int8_t = -1, int8_t = -1) {}
  bool setSCK(int) { return true; }
  bool setTX(int) { return true; }
  bool setRX(int) { return true; }
};

extern SPIClass SPI;
extern SPIClass SPI1;
