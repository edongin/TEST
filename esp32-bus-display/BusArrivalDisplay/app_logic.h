// 화면 전환 규칙 (Arduino 없이 PC에서도 테스트 가능)
#pragma once

#include <stdint.h>

namespace app {

// 터치하지 않았을 때의 기본 화면
enum HomeMode {
  HOME_OFF,       // 화면 꺼짐
  HOME_TODAY,     // 오늘 날씨
  HOME_TOMORROW,  // 내일 날씨
};

struct Schedule {
  int todayStart, todayEnd;        // 오늘 날씨를 보여줄 시간 (시작 이상, 끝 미만)
  int tomorrowStart, tomorrowEnd;  // 내일 날씨를 보여줄 시간
};

// hour: 0~23, timeKnown: 인터넷 시계가 맞춰졌는지
// 시간을 아직 모르면 오늘 날씨를 보여 줍니다.
HomeMode homeModeAt(const Schedule& s, int hour, bool timeKnown);

// 화면 번호: -1 = 기본 화면(날씨 또는 꺼짐), 0 ~ stationCount-1 = 정류장,
//            stationCount = 터치로 띄운 날씨 (extraWeather 일 때만)
// 터치하면 기본 → 정류장1 → 정류장2 → 정류장3 → 기본 순서로 돌아갑니다.
// extraWeather (기본 화면이 꺼져 있는 시간): 정류장3 다음에 날씨를 한 번 더 넣습니다.
//   꺼짐 → 정류장1 → 정류장2 → 정류장3 → 날씨 → 꺼짐
int nextScreen(int screen, int stationCount, bool extraWeather = false);

// 날씨 시간이 아닐 때 터치로 띄우는 날씨가 오늘 것인지 내일 것인지
// (내일 날씨 시작 시각부터 자정까지는 내일, 그 밖에는 오늘)
HomeMode touchWeatherAt(const Schedule& s, int hour, bool timeKnown);

// 정류장 화면을 띄운 뒤 지난 시간이 제한을 넘었는지
bool stationTimedOut(uint32_t nowMs, uint32_t shownAtMs, uint32_t limitMin);

}  // namespace app
