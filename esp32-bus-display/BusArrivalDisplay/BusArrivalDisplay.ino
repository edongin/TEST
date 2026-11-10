// =============================================================================
//  ESP32 버스 도착 알림판
//  공공데이터포털 버스 도착정보 API 를 불러와서 ILI9341 화면에 보여줍니다.
//  설정은 config.h 에서 합니다 (config.example.h 를 복사해서 만드세요).
// =============================================================================
#include <WiFi.h>
#include <time.h>

#include "app_config.h"
#include "bus_api.h"
#include "bus_fetch.h"
#include "display.h"

static const char* APP_TITLE = "버스 도착 알림판";
static const uint32_t RETRY_AFTER_ERROR_MS = 30 * 1000UL;
static const uint32_t STALE_AFTER_SEC = 10 * 60;  // 10분 넘게 갱신 못 하면 경고

// 결과가 4KB 정도라서 스택 대신 전역 변수로 둡니다.
static bus::Result current;   // 화면에 보여주는 정보
static bus::Result incoming;  // 새로 받는 중인 정보 (성공했을 때만 current 로 옮김)
static bool haveData = false;

static uint32_t dataFetchedAtMs = 0;
static uint32_t nextFetchAtMs = 0;
static uint32_t lastReconnectMs = 0;
static uint32_t lastButtonMs = 0;
static bool sleeping = false;

static char footer[48] = "";
static char warning[112] = "";  // 마지막 갱신이 실패한 이유 (성공하면 지움)
static bool wifiLost = false;

// ---------------------------------------------------------------------------

static const char* stationTitle() {
  if (STATION_LABEL[0]) return STATION_LABEL;
  if (current.station[0]) return current.station;
  return "정류장 " STATION_ID;
}

static bool timeIsSet(struct tm* now) {
  // NTP 로 시간을 받기 전에는 1970년이라서 연도로 확인합니다.
  return getLocalTime(now, 0) && now->tm_year >= (2024 - 1900);
}

static bool inActiveHours() {
  struct tm now;
  if (!timeIsSet(&now)) return true;  // 시간을 모르면 일단 켜 둠
  int start = ACTIVE_START_HOUR, end = ACTIVE_END_HOUR;
  if (start == end || (start == 0 && end == 24)) return true;
  if (start < end) return now.tm_hour >= start && now.tm_hour < end;
  return now.tm_hour >= start || now.tm_hour < end;  // 예: 22시 ~ 다음날 2시
}

static void updateFooterTime() {
  struct tm now;
  if (timeIsSet(&now)) {
    strftime(footer, sizeof(footer), "%H:%M:%S 갱신", &now);
  } else {
    snprintf(footer, sizeof(footer), "방금 갱신");
  }
}

static void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  displayMessage(APP_TITLE, "Wi-Fi 연결 중...", WIFI_SSID);
  Serial.printf("[Wi-Fi] %s 에 연결 중", WIFI_SSID);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[Wi-Fi] 연결됨, IP %s\n", WiFi.localIP().toString().c_str());
    displayMessage(APP_TITLE, "도착 정보 불러오는 중...", stationTitle());
  } else {
    Serial.println("[Wi-Fi] 연결 실패, 계속 다시 시도합니다");
    displayMessage(APP_TITLE, "Wi-Fi 연결 실패", "이름/비밀번호, 2.4GHz 인지 확인하세요");
  }
}

static void doFetch() {
  uint32_t now = millis();
  if (fetchArrivals(incoming)) {
    current = incoming;
    haveData = true;
    dataFetchedAtMs = now;
    warning[0] = '\0';
    updateFooterTime();
    nextFetchAtMs = now + UPDATE_INTERVAL_SEC * 1000UL;
    return;
  }

  nextFetchAtMs = now + min((uint32_t)(UPDATE_INTERVAL_SEC * 1000UL), RETRY_AFTER_ERROR_MS);
  if (haveData) {
    // 예전 정보는 계속 보여주고, 아래쪽에 경고만 띄웁니다.
    snprintf(warning, sizeof(warning), "갱신 실패: %s", incoming.error);
  } else {
    displayMessage(APP_TITLE, incoming.error, "30초 뒤 다시 시도합니다");
  }
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== 버스 도착 알림판 시작 ===");

  if (REFRESH_BUTTON_PIN >= 0) pinMode(REFRESH_BUTTON_PIN, INPUT_PULLUP);

  displayBegin();
  connectWifi();

  // 한국 시간(KST, UTC+9) 으로 시계 맞추기
  configTzTime("KST-9", "pool.ntp.org", "time.google.com");
  nextFetchAtMs = millis();
}

void loop() {
  uint32_t now = millis();

  // 1) 밤에는 화면을 끄고 API 도 부르지 않습니다.
  if (!inActiveHours()) {
    if (!sleeping) {
      Serial.println("[화면] 쉬는 시간이라 화면을 끕니다");
      displaySetSleep(true);
      sleeping = true;
    }
    delay(1000);
    return;
  }
  if (sleeping) {
    Serial.println("[화면] 다시 켭니다");
    displaySetSleep(false);
    sleeping = false;
    nextFetchAtMs = now;  // 켜지자마자 새 정보
  }

  // 2) Wi-Fi 가 끊기면 10초마다 다시 연결
  bool online = WiFi.status() == WL_CONNECTED;
  if (!online && now - lastReconnectMs > 10000) {
    lastReconnectMs = now;
    Serial.println("[Wi-Fi] 끊김, 다시 연결 시도");
    WiFi.reconnect();
  }
  if (online && wifiLost) {
    Serial.println("[Wi-Fi] 다시 연결됨");
    nextFetchAtMs = now;
  }
  wifiLost = !online;

  // 3) 버튼을 누르면 바로 새로고침
  if (REFRESH_BUTTON_PIN >= 0 && digitalRead(REFRESH_BUTTON_PIN) == LOW &&
      now - lastButtonMs > 1000) {
    lastButtonMs = now;
    nextFetchAtMs = now;
    Serial.println("[버튼] 새로고침");
  }

  // 4) 정해진 간격마다 API 호출
  if (online && (int32_t)(now - nextFetchAtMs) >= 0) {
    doFetch();
    now = millis();
  }

  // 5) 화면 그리기 (바뀐 곳만 다시 그림)
  if (haveData) {
    uint32_t ageSec = (now - dataFetchedAtMs) / 1000;
    char shown[112];
    if (wifiLost) {
      snprintf(shown, sizeof(shown), "Wi-Fi 끊김, 다시 연결 중...");
    } else if (warning[0]) {
      snprintf(shown, sizeof(shown), "%s", warning);
    } else if (ageSec > STALE_AFTER_SEC) {
      snprintf(shown, sizeof(shown), "%lu분 전 정보입니다", (unsigned long)(ageSec / 60));
    } else {
      shown[0] = '\0';
    }
    int page = (int)((now / 1000) / PAGE_INTERVAL_SEC);
    displayArrivals(current, ageSec, stationTitle(), footer, shown, page);
  }

  delay(100);
}
