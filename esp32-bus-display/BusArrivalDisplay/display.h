// ILI9341 (320x240, 2.8인치/3.2인치) 화면에 버스 도착 정보를 그립니다.
#pragma once

#include <stdint.h>

#include "bus_api.h"
#include "weather_fetch.h"

constexpr int ROWS_PER_PAGE = 5;

void displayBegin();

// 화면 전체에 안내 문구를 보여줍니다 (Wi-Fi 연결 중, 오류 등).
void displayMessage(const char* title, const char* line1, const char* line2 = nullptr);

// 도착 정보 화면. 바뀐 부분만 다시 그려서 깜빡임이 적습니다.
//   ageSec    : 정보를 받은 뒤 지난 시간(초). 남은 시간을 줄여서 보여주는 데 씁니다.
//   station   : 맨 위 제목
//   footer    : 맨 아래 왼쪽 문구 (예: "14:02 갱신")
//   warning   : 비어 있지 않으면 footer 대신 빨간 글씨로 보여줍니다.
//   page      : 노선이 많을 때 몇 번째 페이지인지 (0부터)
void displayArrivals(const bus::Result& data, uint32_t ageSec, const char* station,
                     const char* footer, const char* warning, int page,
                     const char* touchHint);

// 날씨 화면. 바뀐 부분만 다시 그립니다.
//   title     : 맨 위 제목 (예: "오늘 날씨 11/17(화)")
//   nowHour   : 지금 시각(시). 오늘 날씨에서 시간별 예보를 어디서부터 보여줄지 정함
//   footer    : 맨 아래 왼쪽 안내, warning: 빨간 경고 (비어 있으면 안 보임)
//   touchHint : 맨 아래 오른쪽 안내 (예: "터치: 안양대")
void displayWeather(const WeatherData& w, const char* title, int nowHour, const char* footer,
                    const char* warning, const char* touchHint);

// 맨 위 제목 줄(시계 포함)만 다시 그립니다. 바뀐 게 없으면 아무것도 안 함.
void displayHeader(const char* title);

// 화면을 비웁니다 (다른 종류의 화면으로 바꿀 때)
void displayClear();

// 화면 끄기/켜기 (백라이트가 GPIO 에 연결되어 있으면 빛까지 완전히 끔)
void displaySetSleep(bool sleep);
