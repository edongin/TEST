#include "app_logic.h"

namespace app {

static bool inRange(int hour, int start, int end) {
  if (start == end) return false;
  if (start < end) return hour >= start && hour < end;
  return hour >= start || hour < end;  // 자정을 넘는 경우 (예: 22시 ~ 다음날 2시)
}

HomeMode homeModeAt(const Schedule& s, int hour, bool timeKnown) {
  if (!timeKnown) return HOME_TODAY;
  if (inRange(hour, s.todayStart, s.todayEnd)) return HOME_TODAY;
  if (inRange(hour, s.tomorrowStart, s.tomorrowEnd)) return HOME_TOMORROW;
  return HOME_OFF;
}

int nextScreen(int screen, int stationCount) {
  if (stationCount <= 0) return -1;
  if (screen < 0) return 0;
  if (screen + 1 < stationCount) return screen + 1;
  return -1;
}

bool stationTimedOut(uint32_t nowMs, uint32_t shownAtMs, uint32_t limitMin) {
  return (uint32_t)(nowMs - shownAtMs) >= limitMin * 60UL * 1000UL;
}

}  // namespace app
