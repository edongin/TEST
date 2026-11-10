// 도착 정보를 화면에 쓸 문장으로 바꿉니다. (Arduino 없이 PC에서도 테스트 가능)
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bus_api.h"

namespace bus {

enum Urgency {
  URGENCY_NONE,    // 정보 없음 (회색)
  URGENCY_SOON,    // 3분 이내 (주황색)
  URGENCY_NORMAL,  // 그 외 (흰색)
};

// 정보를 받은 뒤 ageSec 초가 지났을 때 남은 시간. 정보가 없으면 -1.
int32_t remainingSec(const Arrival& a, uint32_t ageSec);

// 첫 번째 버스: "3분 (2정거장 전) 23석", "곧 도착", "운행종료", "도착 정보 없음"
Urgency formatFirst(const Arrival& a, uint32_t ageSec, char* out, size_t outSize);

// 두 번째 버스: "다음 15분", "다음 운행종료", 정보 없으면 빈 문자열
void formatSecond(const Arrival& a, uint32_t ageSec, char* out, size_t outSize);

}  // namespace bus
