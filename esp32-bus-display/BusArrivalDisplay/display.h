// 2.8인치 ILI9341 (320x240) 화면에 버스 도착 정보를 그립니다.
#pragma once

#include <stdint.h>

#include "bus_api.h"

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
                     const char* footer, const char* warning, int page);

// 밤 시간 등에 화면 끄기/켜기
void displaySetSleep(bool sleep);
