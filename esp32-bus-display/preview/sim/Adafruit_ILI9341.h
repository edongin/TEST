// PC 미리보기용 가짜 ILI9341 화면.
// 진짜 라이브러리처럼 Adafruit_GFX 를 상속하지만, SPI 로 보내는 대신
// 메모리 속 그림(프레임버퍼)에 점을 찍습니다. 미리보기 프로그램이 이걸 PNG 로 저장합니다.
#pragma once

#include <Adafruit_GFX.h>
#include <SPI.h>

#define ILI9341_TFTWIDTH 240
#define ILI9341_TFTHEIGHT 320

#define ILI9341_SLPIN 0x10
#define ILI9341_SLPOUT 0x11
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON 0x29

#define ILI9341_BLACK 0x0000
#define ILI9341_NAVY 0x000F
#define ILI9341_DARKGREEN 0x03E0
#define ILI9341_DARKCYAN 0x03EF
#define ILI9341_MAROON 0x7800
#define ILI9341_PURPLE 0x780F
#define ILI9341_OLIVE 0x7BE0
#define ILI9341_LIGHTGREY 0xC618
#define ILI9341_DARKGREY 0x7BEF
#define ILI9341_BLUE 0x001F
#define ILI9341_GREEN 0x07E0
#define ILI9341_CYAN 0x07FF
#define ILI9341_RED 0xF800
#define ILI9341_MAGENTA 0xF81F
#define ILI9341_YELLOW 0xFFE0
#define ILI9341_WHITE 0xFFFF
#define ILI9341_ORANGE 0xFD20
#define ILI9341_GREENYELLOW 0xAFE5
#define ILI9341_PINK 0xFC18

class Adafruit_ILI9341 : public Adafruit_GFX {
 public:
  Adafruit_ILI9341(SPIClass*, int8_t, int8_t, int8_t = -1);
  Adafruit_ILI9341(int8_t, int8_t, int8_t = -1);

  void begin(uint32_t = 0);
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void invertDisplay(bool i) override { inverted_ = i; }
  void sendCommand(uint8_t cmd, const uint8_t* = nullptr, uint8_t = 0);
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  // ----- 미리보기 프로그램이 쓰는 부분 (진짜 라이브러리에는 없음) -----
  uint16_t pixel(int16_t x, int16_t y) const;  // 지금 회전 기준 좌표
  bool inverted() const { return inverted_; }
  bool sleeping() const { return sleeping_; }

 private:
  uint16_t fb_[ILI9341_TFTWIDTH * ILI9341_TFTHEIGHT];
  bool inverted_ = false;
  bool sleeping_ = false;
};

// display.cpp 안의 화면 객체를 미리보기 프로그램이 찾을 수 있게 (sim_tft.cpp 에서 등록)
Adafruit_ILI9341* simScreen();
