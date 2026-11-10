#include "platform.h"

#include <Arduino.h>
#include <WiFi.h>
#include <stdlib.h>

#include "app_config.h"

static const char* const NTP_SERVER_1 = "pool.ntp.org";
static const char* const NTP_SERVER_2 = "time.google.com";
static const char* const TIMEZONE = "KST-9";  // 한국 표준시 (UTC+9)

static bool clockStarted = false;

// ===========================================================================
#if defined(ARDUINO_ARCH_RP2040)
// Raspberry Pi Pico W (SB Components "UHF Reader for Pico W" 보드 포함)
// ===========================================================================

const char* platformBoardName() { return "Raspberry Pi Pico W"; }

void platformBoardInit() {
#if UHF_READER_BOARD
  // 이 프로젝트에서는 RFID 기능을 쓰지 않으므로 꺼서 전기와 열을 줄입니다.
  pinMode(UHF_EN_PIN, OUTPUT);
  digitalWrite(UHF_EN_PIN, HIGH);  // HIGH = RFID 모듈 끔
  pinMode(UHF_BUZZER_PIN, OUTPUT);
  digitalWrite(UHF_BUZZER_PIN, LOW);  // 부저 조용히
  pinMode(UHF_LCD_BL_PIN, OUTPUT);
  digitalWrite(UHF_LCD_BL_PIN, LOW);  // 보드에 붙은 1.14인치 작은 화면 백라이트 끔
  pinMode(UHF_SD_CS_PIN, OUTPUT);
  digitalWrite(UHF_SD_CS_PIN, HIGH);  // SD 카드 선택 해제
#endif
}

void platformWifiBegin(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
}

void platformWifiReconnect(const char* ssid, const char* password) {
  WiFi.disconnect();
  WiFi.begin(ssid, password);
}

void platformStartClock() {
  if (clockStarted) return;
  setenv("TZ", TIMEZONE, 1);
  tzset();
  NTP.begin(NTP_SERVER_1, NTP_SERVER_2);
  clockStarted = true;
}

bool platformLocalTime(struct tm* out) {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // 2023년 이전이면 아직 시계가 안 맞은 것
  localtime_r(&now, out);
  return true;
}

// ===========================================================================
#else
// ESP32
// ===========================================================================

const char* platformBoardName() { return "ESP32"; }

void platformBoardInit() {}

void platformWifiBegin(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
}

void platformWifiReconnect(const char* ssid, const char* password) {
  (void)ssid;
  (void)password;
  WiFi.reconnect();
}

void platformStartClock() {
  if (clockStarted) return;
  configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
  clockStarted = true;
}

bool platformLocalTime(struct tm* out) {
  return getLocalTime(out, 0) && out->tm_year >= (2024 - 1900);
}

#endif
