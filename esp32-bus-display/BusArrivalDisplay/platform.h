// 보드(ESP32 / Pico W)마다 다른 부분을 모아 둔 곳입니다.
#pragma once

#include <time.h>

// 전원을 켠 직후 보드 준비 (버튼 핀 설정 등)
void platformBoardInit();

// 터치 대신 쓰는 버튼이 눌려 있으면 true (config.h 의 NEXT_BUTTON_PIN / USE_BOOTSEL_BUTTON)
bool platformButtonPressed();

// Wi-Fi 연결 시작 / 끊겼을 때 다시 연결
void platformWifiBegin(const char* ssid, const char* password);
void platformWifiReconnect(const char* ssid, const char* password);

// 인터넷 시계(NTP) 맞추기 시작. Wi-Fi 가 연결된 뒤 불러야 하며, 여러 번 불러도 됩니다.
void platformStartClock();

// 한국 시간. 아직 시계가 안 맞았으면 false.
bool platformLocalTime(struct tm* out);

// 보드 이름 (시리얼 로그용)
const char* platformBoardName();
