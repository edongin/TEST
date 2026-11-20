#include "touch.h"

#include <Arduino.h>
#include <SPI.h>

#include "app_config.h"
#include "platform.h"

// XPT2046 명령 (S=1, 12비트, 차동 측정)
static const uint8_t CMD_Z1 = 0xB1;           // 누르는 힘 1 (측정 중 켜 둠)
static const uint8_t CMD_Z2 = 0xC1;           // 누르는 힘 2
static const uint8_t CMD_IDLE_IRQ_ON = 0x90;  // 측정 후 절전 + 터치 알림(T_IRQ) 켜기
static const int PRESSURE_THRESHOLD = 400;    // 이보다 세게 누르면 터치로 봄

#if TOUCH_SHARED_SPI
// Pico: 화면과 같은 SPI 통로 (display.cpp 가 이미 시작해 둠)
#define touchSpi SPI
#elif defined(ARDUINO_ARCH_RP2040)
// Pico: 터치는 두 번째 SPI 통로(SPI1)에 따로
#define touchSpi SPI1
#else
// ESP32: 화면이 안 쓰는 다른 SPI 통로
#if TFT_USE_HSPI || !defined(HSPI)
static SPIClass touchSpi(VSPI);
#else
static SPIClass touchSpi(HSPI);
#endif
#endif

static const SPISettings TOUCH_SPI_SETTINGS(2000000, MSBFIRST, SPI_MODE0);

static uint16_t readValue(uint8_t command) {
  touchSpi.transfer(command);
  uint16_t hi = touchSpi.transfer(0);
  uint16_t lo = touchSpi.transfer(0);
  return (uint16_t)(((hi << 8) | lo) >> 3);  // 12비트 값
}

// 누르는 힘을 직접 읽습니다 (TOUCH_MODE 2).
static int readPressure() {
  touchSpi.beginTransaction(TOUCH_SPI_SETTINGS);
  digitalWrite(TOUCH_CS, LOW);
  int z1 = readValue(CMD_Z1);
  int z2 = readValue(CMD_Z2);
  readValue(CMD_IDLE_IRQ_ON);
  digitalWrite(TOUCH_CS, HIGH);
  touchSpi.endTransaction();
  // 터치 칩이 대답하지 않으면(선이 빠졌거나 핀 설정이 다름) 두 값이 똑같이 나옵니다.
  // 이때는 "누름"으로 보지 않아야 화면이 한 번 넘어간 뒤 멈추지 않습니다.
  if (z1 == z2) return 0;
  return z1 + 4095 - z2;
}

void touchBegin() {
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  if (TOUCH_IRQ >= 0) pinMode(TOUCH_IRQ, INPUT_PULLUP);
#if !TOUCH_SHARED_SPI && defined(ARDUINO_ARCH_RP2040)
  touchSpi.setSCK(TOUCH_SCLK);
  touchSpi.setTX(TOUCH_MOSI);
  touchSpi.setRX(TOUCH_MISO);
  touchSpi.begin();
#elif !TOUCH_SHARED_SPI
  touchSpi.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
#endif
  // 터치 칩을 "절전 + 터치하면 T_IRQ 알림" 상태로 맞춰 둡니다.
  touchSpi.beginTransaction(TOUCH_SPI_SETTINGS);
  digitalWrite(TOUCH_CS, LOW);
  readValue(CMD_IDLE_IRQ_ON);
  digitalWrite(TOUCH_CS, HIGH);
  touchSpi.endTransaction();
}

static bool rawPressed() {
  if (platformButtonPressed()) return true;
#if TOUCH_MODE == 2
  return readPressure() > PRESSURE_THRESHOLD;
#else
  // 화면·터치 테스트와 같은 방식: T_IRQ 신호 또는 누르는 힘 중 하나라도 감지되면 터치
  if (TOUCH_IRQ >= 0 && digitalRead(TOUCH_IRQ) == LOW) return true;
  return readPressure() > PRESSURE_THRESHOLD;
#endif
}

// 잡음으로 잘못 눌리지 않게 40ms 동안 계속 눌려 있어야 터치로 봅니다.
// 한 번 터치한 뒤에는 손을 150ms 이상 뗐다가 다시 눌러야 다음 터치가 됩니다.
bool touchTapped() {
  static uint32_t pressStart = 0;
  static uint32_t releaseStart = 0;
  static bool waitRelease = false;
  static uint32_t lastPoll = 0;

  uint32_t now = millis();
  if (now - lastPoll < 25) return false;  // 25ms 마다 확인
  lastPoll = now;

  bool pressed = rawPressed();

  if (waitRelease) {
    if (pressed) {
      releaseStart = 0;
    } else if (releaseStart == 0) {
      releaseStart = now;
    } else if (now - releaseStart >= 150) {
      waitRelease = false;
      pressStart = 0;
    }
    return false;
  }

  if (!pressed) {
    pressStart = 0;
    return false;
  }
  if (pressStart == 0) {
    pressStart = now;
    return false;
  }
  if (now - pressStart >= 40) {
    waitRelease = true;
    releaseStart = 0;
    return true;
  }
  return false;
}
