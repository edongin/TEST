// =============================================================================
//  버스 도착 + 날씨 알림판 (Raspberry Pi Pico W / ESP32)
//
//  - 평소(기본 화면): 오전에는 오늘 날씨, 저녁에는 내일 날씨, 그 밖에는 화면 꺼짐
//  - 화면을 터치할 때마다: 정류장1 → 정류장2 → 정류장3 → 기본 화면
//    (화면이 꺼져 있는 시간에는 정류장3 다음에 날씨가 한 번 더 나옴)
//  - 정류장 화면에서는 15초마다 도착 정보를 새로 받고, 30분이 지나면 기본 화면으로
//
//  설정은 config.h 에서 합니다 (config.example.h 를 복사해서 만드세요).
// =============================================================================
#include <WiFi.h>
#include <time.h>

#include "app_config.h"
#include "app_logic.h"
#include "bus_api.h"
#include "bus_fetch.h"
#include "display.h"
#include "platform.h"
#include "touch.h"
#include "weather_fetch.h"

static const char* APP_TITLE = "버스와 날씨 알림판";
static const uint32_t BUS_STALE_SEC = 2 * 60;          // 2분 넘게 갱신 못 하면 경고
static const uint32_t BUS_FIRST_RETRY_MS = 5000;     // 첫 정보를 못 받았을 때 다시 시도 간격
static const uint32_t WEATHER_RETRY_MS = 2 * 60000UL;  // 날씨 실패 시 다시 시도 간격
static const char* const WEEKDAYS[] = {"일", "월", "화", "수", "목", "금", "토"};

static const app::Schedule SCHEDULE = {TODAY_WEATHER_START, TODAY_WEATHER_END,
                                       TOMORROW_WEATHER_START, TOMORROW_WEATHER_END};

// 지금 보여줄 화면 종류: 0 ~ STATION_COUNT-1 은 정류장
enum { KIND_TODAY = 100, KIND_TOMORROW, KIND_OFF };

// ----- 화면 상태 -----
static int screen = -1;  // -1 = 기본 화면, 0~ = 정류장 번호, STATION_COUNT = 터치로 띄운 날씨
static uint32_t stationShownAtMs = 0;
static bool displayOn = true;
static int drawnKind = -1;  // 마지막으로 그린 화면 종류

// ----- 버스 -----
static bus::Result busData;      // 화면에 보여주는 정보
static bus::Result busIncoming;  // 새로 받는 중인 정보 (성공했을 때만 busData 로 옮김)
static bool haveBus = false;
static uint32_t busFetchedAtMs = 0, nextBusFetchMs = 0;
static char busFooterTime[24] = "";
static char busError[112] = "";

// ----- 날씨 -----
static WeatherData weatherData, weatherIncoming;
static bool haveWeather = false;
static uint32_t nextWeatherFetchMs = 0;
static char weatherFooter[64] = "";
static char weatherError[112] = "";

// ----- Wi-Fi -----
static uint32_t lastReconnectMs = 0;
static bool wifiLost = false;

// ---------------------------------------------------------------------------

static void stationTitle(int index, char* out, size_t size) {
  const StationConfig& st = STATIONS[index];
  const char* name = st.label[0] ? st.label : (busData.station[0] ? busData.station : st.id);
  snprintf(out, size, "%d/%d  %s", index + 1, STATION_COUNT, name);
}

// 정류장의 짧은 이름: "안양대(안양역 방향)" -> "안양대"
// 짧게 줄인 이름이 다른 정류장과 겹치면 원래 이름을 그대로 씁니다.
static void shortStationName(int index, char* out, size_t size) {
  const StationConfig& st = STATIONS[index];
  if (!st.label[0]) {
    snprintf(out, size, "정류장 %d", index + 1);
    return;
  }
  auto shorten = [](const char* label, char* buf, size_t n) {
    size_t len = strcspn(label, "(");
    while (len > 0 && label[len - 1] == ' ') len--;
    if (len == 0) len = strlen(label);
    if (len >= n) len = n - 1;
    memcpy(buf, label, len);
    buf[len] = '\0';
  };
  shorten(st.label, out, size);
  for (int i = 0; i < STATION_COUNT; i++) {
    if (i == index || !STATIONS[i].label[0]) continue;
    char other[64];
    shorten(STATIONS[i].label, other, sizeof(other));
    if (strcmp(other, out) == 0) {
      snprintf(out, size, "%s", st.label);
      return;
    }
  }
}

// 지금이 기본 화면이 꺼져 있는 시간인지 (그러면 터치 순서에 날씨를 한 번 더 넣음)
static bool homeIsOff() {
  struct tm t;
  bool timeKnown = platformLocalTime(&t);
  return app::homeModeAt(SCHEDULE, timeKnown ? t.tm_hour : 0, timeKnown) == app::HOME_OFF;
}

// 터치로 띄운 날씨가 오늘 것인지 내일 것인지
static bool touchWeatherIsTomorrow() {
  struct tm t;
  bool timeKnown = platformLocalTime(&t);
  return app::touchWeatherAt(SCHEDULE, timeKnown ? t.tm_hour : 0, timeKnown) == app::HOME_TOMORROW;
}

// 맨 아래 오른쪽 안내: 터치하면 나올 화면 이름 (예: "터치: 안양대", "터치: 오늘 날씨")
static void touchHintFor(int current, char* out, size_t size) {
  int next = app::nextScreen(current, STATION_COUNT, homeIsOff());
  if (next >= STATION_COUNT) {
    snprintf(out, size, "터치: %s", touchWeatherIsTomorrow() ? "내일 날씨" : "오늘 날씨");
    return;
  }
  if (next >= 0) {
    char name[64];
    shortStationName(next, name, sizeof(name));
    snprintf(out, size, "터치: %s", name);
    return;
  }
  struct tm t;
  bool timeKnown = platformLocalTime(&t);
  switch (app::homeModeAt(SCHEDULE, timeKnown ? t.tm_hour : 0, timeKnown)) {
    case app::HOME_TODAY: snprintf(out, size, "터치: 오늘 날씨"); break;
    case app::HOME_TOMORROW: snprintf(out, size, "터치: 내일 날씨"); break;
    default: snprintf(out, size, "터치: 화면 끄기"); break;
  }
}

static void clockText(const char* format, char* out, size_t size) {
  struct tm now;
  if (platformLocalTime(&now)) {
    strftime(out, size, format, &now);
  } else {
    out[0] = '\0';
  }
}

static void connectWifi() {
  displayMessage(APP_TITLE, "Wi-Fi 연결 중...", WIFI_SSID);
  platformWifiBegin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[Wi-Fi] %s 에 연결 중", WIFI_SSID);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[Wi-Fi] 연결됨, IP %s\n", WiFi.localIP().toString().c_str());
    platformStartClock();
    displayMessage(APP_TITLE, "시계 맞추는 중...", nullptr);
    for (int i = 0; i < 20; i++) {  // 최대 10초 기다림 (못 맞춰도 계속 진행)
      struct tm t;
      if (platformLocalTime(&t)) break;
      delay(500);
    }
  } else {
    Serial.println("[Wi-Fi] 연결 실패, 계속 다시 시도합니다");
    displayMessage(APP_TITLE, "Wi-Fi 연결 실패", "이름/비밀번호, 2.4GHz 인지 확인하세요");
    delay(3000);
  }
}

// ---------------------------------------------------------------------------
// 정류장 화면
// ---------------------------------------------------------------------------

static void fetchBus(int index, uint32_t now) {
  nextBusFetchMs = now + BUS_UPDATE_SEC * 1000UL;
  if (fetchArrivals(STATIONS[index], busIncoming)) {
    busData = busIncoming;
    haveBus = true;
    busFetchedAtMs = millis();
    busError[0] = '\0';
    clockText("%H:%M:%S 갱신", busFooterTime, sizeof(busFooterTime));
    return;
  }
  snprintf(busError, sizeof(busError), "갱신 실패: %s", busIncoming.error);
  if (!haveBus) {
    // 아직 보여줄 정보가 없으면 15초까지 기다리지 않고 5초 뒤에 다시 받습니다.
    nextBusFetchMs = millis() + BUS_FIRST_RETRY_MS;
    char title[96];
    stationTitle(index, title, sizeof(title));
    displayMessage(title, busIncoming.error, "5초 뒤 다시 시도합니다");
  }
}

static void showStation(int index, bool firstDraw, bool online, uint32_t now) {
  char title[96];
  stationTitle(index, title, sizeof(title));

  if (firstDraw) {
    displayMessage(title, online ? "도착 정보 불러오는 중..." : "Wi-Fi 다시 연결 중...", nullptr);
  }
  if (online && (int32_t)(now - nextBusFetchMs) >= 0) {
    fetchBus(index, now);
    now = millis();
  }
  if (!haveBus) {
    displayHeader(title);  // 오류 화면에서도 시계는 계속 바뀌게
    return;
  }

  uint32_t ageSec = (now - busFetchedAtMs) / 1000;
  char hint[96];
  touchHintFor(index, hint, sizeof(hint));

  char warning[112] = "";
  if (wifiLost) {
    snprintf(warning, sizeof(warning), "Wi-Fi 끊김, 다시 연결 중...");
  } else if (busError[0] && ageSec > BUS_STALE_SEC) {
    snprintf(warning, sizeof(warning), "%s", busError);
  }

  int page = (int)((now / 1000) / PAGE_INTERVAL_SEC);
  displayArrivals(busData, ageSec, title, busFooterTime, warning, page, hint);
}

// ---------------------------------------------------------------------------
// 날씨 화면
// ---------------------------------------------------------------------------

static void weatherTitle(bool tomorrow, const WeatherData* w, char* out, size_t size) {
  const char* what = tomorrow ? "내일 날씨" : "오늘 날씨";
  if (w && w->ok) {
    snprintf(out, size, "%s  %d/%d(%s)", what, w->month, w->mday, WEEKDAYS[w->wday % 7]);
  } else {
    snprintf(out, size, "%s", what);
  }
}

static void showWeather(bool tomorrow, bool firstDraw, bool online, uint32_t now) {
  struct tm t;
  bool timeKnown = platformLocalTime(&t);
  char title[96];

  // 받아 둔 날씨가 지금 보여줄 날짜와 맞는지 (오늘↔내일, 날짜가 바뀐 경우)
  if (haveWeather) {
    bool matches = timeKnown && weatherData.tomorrow == tomorrow;
    if (matches) {
      char target[9];
      weather::dateAfter(t, tomorrow ? 1 : 0, target, nullptr);
      matches = strcmp(target, weatherData.day.date) == 0;
    }
    if (!matches) {
      haveWeather = false;
      nextWeatherFetchMs = now;  // 바로 새로 받기
    }
  }

  if (firstDraw && !haveWeather) {
    weatherTitle(tomorrow, nullptr, title, sizeof(title));
    displayMessage(title,
                   !online      ? "Wi-Fi 다시 연결 중..."
                   : !timeKnown ? "시계 맞추는 중..."
                                : "날씨 불러오는 중...",
                   nullptr);
    nextWeatherFetchMs = now;
  }

  if (online && timeKnown && (int32_t)(now - nextWeatherFetchMs) >= 0) {
    if (fetchWeather(tomorrow, weatherIncoming)) {
      bool wasShowing = haveWeather;
      weatherData = weatherIncoming;
      haveWeather = true;
      weatherError[0] = '\0';
      char at[16];
      clockText("%H:%M 갱신", at, sizeof(at));
      snprintf(weatherFooter, sizeof(weatherFooter), "%s  %s", WEATHER_LABEL, at);
      nextWeatherFetchMs = millis() + WEATHER_UPDATE_MIN * 60000UL;
      if (!wasShowing) displayClear();  // "불러오는 중" 글자 지우기
    } else {
      nextWeatherFetchMs = millis() + WEATHER_RETRY_MS;
      snprintf(weatherError, sizeof(weatherError), "날씨 갱신 실패: %s", weatherIncoming.error);
      if (!haveWeather) {
        weatherTitle(tomorrow, nullptr, title, sizeof(title));
        displayMessage(title, weatherIncoming.error, "터치하면 다음 화면");
      }
    }
  }
  if (!haveWeather) {
    weatherTitle(tomorrow, nullptr, title, sizeof(title));
    displayHeader(title);  // 오류 화면에서도 시계는 계속 바뀌게
    return;
  }

  weatherTitle(tomorrow, &weatherData, title, sizeof(title));
  const char* warning = wifiLost ? "Wi-Fi 끊김, 다시 연결 중..." : weatherError;
  char hint[96];
  touchHintFor(screen, hint, sizeof(hint));
  displayWeather(weatherData, title, timeKnown ? t.tm_hour : 12, weatherFooter, warning, hint);
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== 버스와 날씨 알림판 시작 (%s, 정류장 %d곳) ===\n", platformBoardName(),
                STATION_COUNT);

  platformBoardInit();
  displayBegin();
  touchBegin();
  connectWifi();
}

void loop() {
  uint32_t now = millis();

  // 1) Wi-Fi 가 끊기면 20초마다 다시 연결
  bool online = WiFi.status() == WL_CONNECTED;
  if (!online && now - lastReconnectMs > 20000) {
    lastReconnectMs = now;
    Serial.println("[Wi-Fi] 끊김, 다시 연결 시도");
    platformWifiReconnect(WIFI_SSID, WIFI_PASSWORD);
    now = millis();
    online = WiFi.status() == WL_CONNECTED;
  }
  if (online && wifiLost) Serial.println("[Wi-Fi] 다시 연결됨");
  wifiLost = !online;
  if (online) platformStartClock();  // 처음 연결됐을 때 한 번만 실제로 시작됨

  // 2) 터치: 기본 화면 → 정류장1 → 정류장2 → 정류장3 → 기본 화면
  //    기본 화면이 꺼져 있는 시간에는 정류장3 다음에 날씨가 한 번 더 나옵니다.
  if (touchTapped()) {
    screen = app::nextScreen(screen, STATION_COUNT, homeIsOff());
    if (screen >= STATION_COUNT) {
      stationShownAtMs = now;
      Serial.printf("[터치] %s\n", touchWeatherIsTomorrow() ? "내일 날씨" : "오늘 날씨");
    } else if (screen >= 0) {
      stationShownAtMs = now;
      haveBus = false;
      busError[0] = '\0';
      // 바로 받으면 받는 1~2초 동안 다음 터치를 못 읽어서, 잠깐 기다렸다가 받습니다.
      // (정류장을 빠르게 넘길 때 터치가 씹히지 않게)
      nextBusFetchMs = now + 800;
      Serial.printf("[터치] 정류장 %d: %s\n", screen + 1, STATIONS[screen].label);
    } else {
      Serial.println("[터치] 기본 화면");
    }
  }

  // 3) 정류장 화면(과 터치로 띄운 날씨)은 정해진 시간이 지나면 기본 화면으로
  if (screen >= 0 && app::stationTimedOut(now, stationShownAtMs, STATION_SHOW_MIN)) {
    Serial.printf("[화면] %d분이 지나 기본 화면으로\n", STATION_SHOW_MIN);
    screen = -1;
  }

  // 4) 지금 보여줄 화면 정하기
  struct tm t;
  bool timeKnown = platformLocalTime(&t);
  int kind;
  if (screen >= STATION_COUNT) {
    kind = touchWeatherIsTomorrow() ? KIND_TOMORROW : KIND_TODAY;
  } else if (screen >= 0) {
    kind = screen;
  } else {
    switch (app::homeModeAt(SCHEDULE, timeKnown ? t.tm_hour : 0, timeKnown)) {
      case app::HOME_TODAY: kind = KIND_TODAY; break;
      case app::HOME_TOMORROW: kind = KIND_TOMORROW; break;
      default: kind = KIND_OFF; break;
    }
  }

  // 5) 화면 켜기/끄기, 화면 종류가 바뀌면 새로 그리기
  bool wantOn = kind != KIND_OFF;
  if (wantOn != displayOn) {
    Serial.printf("[화면] %s\n", wantOn ? "켜기" : "끄기");
    displaySetSleep(!wantOn);
    displayOn = wantOn;
    drawnKind = -1;
  }
  bool firstDraw = kind != drawnKind;
  if (firstDraw && wantOn) displayClear();
  drawnKind = kind;

  // 6) 화면 그리기
  if (kind < STATION_COUNT) {
    showStation(kind, firstDraw, online, now);
  } else if (kind == KIND_TODAY || kind == KIND_TOMORROW) {
    showWeather(kind == KIND_TOMORROW, firstDraw, online, now);
  }

  delay(10);
}
