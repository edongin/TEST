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
// Raspberry Pi Pico W / Pico WH / Pico 2 W
// ===========================================================================

const char* platformBoardName() { return "Raspberry Pi Pico W"; }

void platformBoardInit() {
  if (NEXT_BUTTON_PIN >= 0) pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
}

bool platformButtonPressed() {
  if (NEXT_BUTTON_PIN >= 0 && digitalRead(NEXT_BUTTON_PIN) == LOW) return true;
#if USE_BOOTSEL_BUTTON
  return BOOTSEL;  // Pico 위의 흰색 BOOTSEL 버튼
#else
  return false;
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

void platformBoardInit() {
  if (NEXT_BUTTON_PIN >= 0) pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
}

bool platformButtonPressed() {
  return NEXT_BUTTON_PIN >= 0 && digitalRead(NEXT_BUTTON_PIN) == LOW;
}

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
