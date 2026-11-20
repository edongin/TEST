// 화면 미리보기 프로그램
//
// 보드에 올리는 것과 똑같은 화면 그리기 코드(BusArrivalDisplay/display.cpp)를 PC에서 돌려서
// 여러 상황의 화면을 PNG 그림으로 저장합니다. 보드·화면·Wi-Fi 없이 모양을 바로 확인할 수 있습니다.
//
//   make                 모든 화면을 out/ 폴더에 저장 (out/all.png 에 한 장으로 모음)
//   ./preview --list     만들 수 있는 화면 이름 보기
//   ./preview bus_many   그 화면만 저장
//   ./preview --scale 1  실제 픽셀 크기(320x240)로 저장 (기본 2배)
//
// 화면에 넣을 내용(버스 도착, 날씨, 시각)을 바꾸고 싶으면 아래 "화면 목록"을 고치세요.
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

#include <functional>
#include <string>
#include <vector>

#include "bus_api.h"
#include "display.h"
#include "sim.h"
#include "weather_fetch.h"

using namespace weather;

// ---------------------------------------------------------------------------
// 예시 데이터 만들기 도우미
// ---------------------------------------------------------------------------

static bus::Arrival arrive(int sec, int stops, int seats = -1, const char* note = "") {
  bus::Arrival a;
  a.sec = sec;
  a.stops = (int16_t)stops;
  a.seats = (int16_t)seats;
  snprintf(a.note, sizeof(a.note), "%s", note);
  return a;
}

static bus::Arrival none(const char* note = "") { return arrive(-1, -1, -1, note); }

static void addRoute(bus::Result& r, const char* name, const char* dest, bus::Arrival first,
                     bus::Arrival second) {
  bus::Route& route = r.routes[r.count++];
  snprintf(route.name, sizeof(route.name), "%s", name);
  snprintf(route.dest, sizeof(route.dest), "%s", dest);
  route.next[0] = first;
  route.next[1] = second;
}

static bus::Result emptyResult() {
  bus::Result r;
  memset(&r, 0, sizeof(r));
  r.ok = true;
  return r;
}

// 시간별 예보 한 줄: 시각, 기온, 하늘(1 맑음 3 구름많음 4 흐림), 강수형태(0 없음 1 비 3 눈), 강수확률
static void setHour(Day& d, int hour, int temp, int sky, int pty, int pop) {
  d.hours[hour] = Hour{(int8_t)temp, (int8_t)sky, (int8_t)pty, (int8_t)pop};
}

static WeatherData sampleWeather(bool tomorrow, int month, int mday, int wday) {
  WeatherData w;
  memset(&w, 0, sizeof(w));
  w.ok = true;
  w.tomorrow = tomorrow;
  w.month = month;
  w.mday = mday;
  w.wday = wday;
  Day& d = w.day;
  d.ok = true;
  snprintf(d.date, sizeof(d.date), "2026%02d%02d", month, mday);
  for (auto& h : d.hours) h = Hour{UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN};
  return w;
}

static const char* const WEEKDAYS[] = {"일", "월", "화", "수", "목", "금", "토"};

static std::string weatherTitle(const WeatherData& w) {
  char buf[96];
  snprintf(buf, sizeof(buf), "%s  %d/%d(%s)", w.tomorrow ? "내일 날씨" : "오늘 날씨", w.month,
           w.mday, WEEKDAYS[w.wday % 7]);
  return buf;
}

// ---------------------------------------------------------------------------
// 화면 목록 (여기를 고치면 미리보기 내용이 바뀝니다)
// ---------------------------------------------------------------------------

struct Scene {
  const char* name;
  const char* about;
  std::function<void()> draw;
};

static const std::vector<Scene> SCENES = {
    {"weather_today", "오늘 날씨 (아침, 오후에 비)",
     [] {
       simSetTime(2026, 11, 20, 8, 12);
       WeatherData w = sampleWeather(false, 11, 20, 5);
       w.day.tmin10 = -18;
       w.day.tmax10 = 94;
       w.now = Now{true, 34, 0, 62};
       const int temps[24] = {1, 0, 0, -1, -1, -2, -2, -1, 2, 4, 6, 7,
                              8, 9, 9, 8, 7, 6, 5, 4, 3, 3, 2, 2};
       for (int h = 0; h < 24; h++) {
         bool rain = h >= 14 && h <= 18;
         setHour(w.day, h, temps[h], h < 11 ? 1 : (rain ? 4 : 3), rain ? 1 : 0,
                 rain ? 70 : (h < 11 ? 0 : 30));
       }
       displayWeather(w, weatherTitle(w).c_str(), 8, "우리 동네  08:10 갱신", "",
                      "터치: 집 앞");
     }},

    {"weather_tomorrow", "내일 날씨 (밤에 보는 화면, 눈)",
     [] {
       simSetTime(2026, 11, 20, 21, 5);
       WeatherData w = sampleWeather(true, 11, 21, 6);
       w.day.tmin10 = -52;
       w.day.tmax10 = 11;
       for (int h = 0; h < 24; h++) {
         bool snow = h >= 9 && h <= 15;
         setHour(w.day, h, -5 + (h > 6 && h < 16 ? (h - 6) * 6 / 9 : 0), snow ? 4 : 3,
                 snow ? 3 : 0, snow ? 60 : 20);
       }
       displayWeather(w, weatherTitle(w).c_str(), 21, "우리 동네  21:00 갱신", "",
                      "터치: 집 앞");
     }},

    {"bus_two", "정류장 화면 - 노선 2개 (크게)",
     [] {
       simSetTime(2026, 11, 20, 7, 42);
       bus::Result r = emptyResult();
       addRoute(r, "11-3", "범계역 방면", arrive(95, 1, 12), arrive(840, 6, 30));
       addRoute(r, "5", "안양역 방면", arrive(420, 3), arrive(1500, 9));
       displayArrivals(r, 3, "1/3  집 앞 (서울 방향)", "07:42:05 갱신", "", 0,
                       "터치: 집 앞 (안양 방향)");
     }},

    {"bus_many", "정류장 화면 - 노선 많음 (작게, 페이지 넘김)",
     [] {
       simSetTime(2026, 11, 20, 7, 42);
       bus::Result r = emptyResult();
       addRoute(r, "11-3", "범계역 방면", arrive(40, 0), arrive(840, 6));
       addRoute(r, "5", "안양역 방면", arrive(185, 2, 4), arrive(1020, 8, 25));
       addRoute(r, "9-3", "인덕원역 방면", arrive(600, 4), none());
       addRoute(r, "3330", "강남역 방면", arrive(960, 7, 0), arrive(2400, 14, 21));
       addRoute(r, "N61", "심야 노선", none("운행종료"), none());
       addRoute(r, "1", "평촌 방면", none("출발대기"), none());
       displayArrivals(r, 3, "2/3  회사 앞", "07:42:05 갱신", "", 0, "터치: 오늘 날씨");
     }},

    {"bus_empty", "정류장 화면 - 도착 정보 없음",
     [] {
       simSetTime(2026, 11, 20, 1, 30);
       bus::Result r = emptyResult();
       displayArrivals(r, 0, "3/3  회사 앞", "01:30:00 갱신", "", 0, "터치: 화면 끄기");
     }},

    {"bus_warning", "정류장 화면 - 갱신 실패 경고",
     [] {
       simSetTime(2026, 11, 20, 18, 3);
       bus::Result r = emptyResult();
       addRoute(r, "11-3", "범계역 방면", arrive(300, 2), arrive(1200, 8));
       addRoute(r, "5", "안양역 방면", arrive(720, 5), none());
       addRoute(r, "9-3", "인덕원역 방면", arrive(60, 0), arrive(900, 6));
       displayArrivals(r, 95, "1/3  집 앞 (서울 방향)", "18:01:30 갱신",
                       "Wi-Fi 끊김, 다시 연결 중...", 0, "터치: 집 앞 (안양 방향)");
     }},

    {"msg_loading", "안내 화면 - 불러오는 중",
     [] {
       simSetTime(2026, 11, 20, 7, 40);
       displayMessage("1/3  집 앞 (서울 방향)", "도착 정보 불러오는 중...", nullptr);
     }},

    {"msg_wifi_fail", "안내 화면 - Wi-Fi 연결 실패 (시계 모름)",
     [] {
       simClearTime();
       displayMessage("버스와 날씨 알림판", "Wi-Fi 연결 실패",
                      "이름/비밀번호, 2.4GHz 인지 확인하세요");
     }},
};

// ---------------------------------------------------------------------------

static void usage() {
  printf("사용법: preview [--scale N] [--out 폴더] [--list] [화면이름 ...]\n");
}

int main(int argc, char** argv) {
  int scale = 2;
  std::string outDir = "out";
  std::vector<std::string> wanted;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--list") {
      for (const auto& s : SCENES) printf("  %-18s %s\n", s.name, s.about);
      return 0;
    } else if (a == "--scale" && i + 1 < argc) {
      scale = atoi(argv[++i]);
      if (scale < 1 || scale > 8) scale = 2;
    } else if (a == "--out" && i + 1 < argc) {
      outDir = argv[++i];
    } else if (a == "-h" || a == "--help") {
      usage();
      return 0;
    } else {
      wanted.push_back(a);
    }
  }

  for (const auto& w : wanted) {
    bool found = false;
    for (const auto& s : SCENES) found |= w == s.name;
    if (!found) {
      fprintf(stderr, "'%s' 라는 화면이 없습니다. --list 로 이름을 확인하세요.\n", w.c_str());
      return 1;
    }
  }

#ifdef _WIN32
  _mkdir(outDir.c_str());
#else
  mkdir(outDir.c_str(), 0755);
#endif

  displayBegin();
  int saved = 0;
  for (const auto& s : SCENES) {
    if (!wanted.empty()) {
      bool pick = false;
      for (const auto& w : wanted) pick |= w == s.name;
      if (!pick) continue;
    }
    displayClear();
    s.draw();
    std::string path = outDir + "/" + s.name + ".png";
    if (!simSavePng(path.c_str(), scale)) {
      fprintf(stderr, "저장 실패: %s\n", path.c_str());
      return 1;
    }
    simCapture(s.name);
    printf("  %-28s %s\n", path.c_str(), s.about);
    saved++;
  }
  if (saved > 1) {
    std::string path = outDir + "/all.png";
    simSaveSheet(path.c_str(), 2, 1);
    printf("  %-28s 모든 화면을 한 장에\n", path.c_str());
  }
  return 0;
}
