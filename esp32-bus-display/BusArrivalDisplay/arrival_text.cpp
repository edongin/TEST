#include "arrival_text.h"

#include <stdio.h>
#include <string.h>

namespace bus {

static const int32_t SOON_SEC = 180;

int32_t remainingSec(const Arrival& a, uint32_t ageSec) {
  if (a.sec < 0) return -1;
  int32_t remain = a.sec - (int32_t)ageSec;
  return remain > 0 ? remain : 0;
}

// "곧 도착" 또는 "N분"
static void formatTime(int32_t remain, char* out, size_t outSize) {
  if (remain <= 60) {
    snprintf(out, outSize, "곧 도착");
  } else {
    snprintf(out, outSize, "%ld분", (long)(remain / 60));
  }
}

Urgency formatFirst(const Arrival& a, uint32_t ageSec, char* out, size_t outSize) {
  int32_t remain = remainingSec(a, ageSec);
  if (remain < 0) {
    copyUtf8(out, outSize, a.note[0] ? a.note : "도착 정보 없음");
    return URGENCY_NONE;
  }

  char time[24];
  formatTime(remain, time, sizeof(time));
  char buf[96];
  int n = snprintf(buf, sizeof(buf), "%s", time);
  if (a.stops > 0 && n > 0 && (size_t)n < sizeof(buf)) {
    n += snprintf(buf + n, sizeof(buf) - n, " (%d정거장 전)", a.stops);
  }
  if (a.seats >= 0 && n > 0 && (size_t)n < sizeof(buf)) {
    snprintf(buf + n, sizeof(buf) - n, " %d석", a.seats);
  }
  copyUtf8(out, outSize, buf);
  return remain <= SOON_SEC ? URGENCY_SOON : URGENCY_NORMAL;
}

void formatSecond(const Arrival& a, uint32_t ageSec, char* out, size_t outSize) {
  int32_t remain = remainingSec(a, ageSec);
  char buf[64];
  if (remain >= 0) {
    char time[24];
    formatTime(remain, time, sizeof(time));
    snprintf(buf, sizeof(buf), "다음 %s", time);
  } else if (a.note[0]) {
    snprintf(buf, sizeof(buf), "다음 %s", a.note);
  } else {
    buf[0] = '\0';
  }
  copyUtf8(out, outSize, buf);
}

}  // namespace bus
