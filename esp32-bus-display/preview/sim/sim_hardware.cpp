// PC 미리보기용 가짜 하드웨어: 화면(프레임버퍼), 시계, 핀.
#include <Adafruit_ILI9341.h>
#include <Arduino.h>
#include <SPI.h>

#include "platform.h"
#include "sim.h"

SPIClass SPI;
SPIClass SPI1;

// ----------------------------------------------------------------- 화면
static Adafruit_ILI9341* g_screen = nullptr;
Adafruit_ILI9341* simScreen() { return g_screen; }

Adafruit_ILI9341::Adafruit_ILI9341(SPIClass*, int8_t, int8_t, int8_t)
    : Adafruit_GFX(ILI9341_TFTWIDTH, ILI9341_TFTHEIGHT) {
  memset(fb_, 0, sizeof(fb_));
  g_screen = this;
}
Adafruit_ILI9341::Adafruit_ILI9341(int8_t, int8_t, int8_t)
    : Adafruit_ILI9341(nullptr, 0, 0, 0) {}

void Adafruit_ILI9341::begin(uint32_t) {}

// 회전한 좌표 그대로 _width x _height 그림에 저장합니다 (PNG 도 그 방향으로 나옴).
void Adafruit_ILI9341::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= _width || y >= _height) return;
  fb_[y * _width + x] = color;
}

uint16_t Adafruit_ILI9341::pixel(int16_t x, int16_t y) const {
  if (x < 0 || y < 0 || x >= _width || y >= _height) return 0;
  return fb_[y * _width + x];
}

void Adafruit_ILI9341::sendCommand(uint8_t cmd, const uint8_t*, uint8_t) {
  if (cmd == ILI9341_DISPOFF) sleeping_ = true;
  if (cmd == ILI9341_DISPON) sleeping_ = false;
}

// ----------------------------------------------------------------- 시계
static struct tm g_time;
static bool g_timeKnown = false;
static uint32_t g_millis = 0;

void simSetTime(int year, int month, int day, int hour, int minute, int second) {
  memset(&g_time, 0, sizeof(g_time));
  g_time.tm_year = year - 1900;
  g_time.tm_mon = month - 1;
  g_time.tm_mday = day;
  g_time.tm_hour = hour;
  g_time.tm_min = minute;
  g_time.tm_sec = second;
  g_time.tm_isdst = -1;
  mktime(&g_time);  // 요일(tm_wday) 채우기
  g_timeKnown = true;
}
void simClearTime() { g_timeKnown = false; }
void simSetMillis(uint32_t ms) { g_millis = ms; }

bool platformLocalTime(struct tm* out) {
  if (!g_timeKnown) return false;
  *out = g_time;
  return true;
}

uint32_t millis() { return g_millis; }
uint32_t micros() { return g_millis * 1000; }
void delay(uint32_t ms) { g_millis += ms; }
void yield() {}
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}
int digitalRead(uint8_t) { return HIGH; }
