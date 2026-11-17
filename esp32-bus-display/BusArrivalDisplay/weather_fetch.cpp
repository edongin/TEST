#include "weather_fetch.h"

#include <Arduino.h>
#include <HTTPClient.h>

#include "app_config.h"
#include "bus_api.h"
#include "net_util.h"
#include "platform.h"

// 인터넷 응답을 한 글자씩 읽어 주는 통로
class StreamByteReader : public weather::ByteReader {
 public:
  explicit StreamByteReader(Stream& s) : s_(s) { s_.setTimeout(8000); }
  int read() override {
    char c;
    return s_.readBytes(&c, 1) == 1 ? (unsigned char)c : -1;
  }

 private:
  Stream& s_;
};

static void gridPosition(int* nx, int* ny) {
  static int cachedX = -1, cachedY = -1;
  if (cachedX < 0) weather::latLonToGrid(WEATHER_LAT, WEATHER_LON, &cachedX, &cachedY);
  *nx = cachedX;
  *ny = cachedY;
}

// 단기예보: 응답이 커서 받으면서 바로 해석합니다.
static bool fetchForecast(const struct tm& now, bool tomorrow, WeatherData& out) {
  char baseDate[9], baseTime[5], target[9];
  weather::forecastBase(now, !tomorrow, baseDate, baseTime);
  weather::dateAfter(now, tomorrow ? 1 : 0, target, &out.wday);
  out.month = (int)strtol(target + 4, nullptr, 10) / 100;
  out.mday = (int)(strtol(target + 6, nullptr, 10));

  int nx, ny;
  gridPosition(&nx, &ny);
  static char url[512];
  if (!weather::buildForecastUrl(SERVICE_KEY, nx, ny, baseDate, baseTime, url, sizeof(url))) {
    bus::copyUtf8(out.error, sizeof(out.error), "설정 확인: 인증키");
    return false;
  }

  Serial.printf("[날씨] %s 예보 요청 (발표 %s %s, 격자 %d,%d)\n", target, baseDate, baseTime,
                nx, ny);
  HTTPClient http;
  http.useHTTP10(true);  // 응답을 나눠 보내지 않게 해서 바로 읽을 수 있도록
  if (!httpBegin(http, url)) {
    bus::copyUtf8(out.error, sizeof(out.error), "요청 주소 오류");
    return false;
  }
  int status = http.GET();
  if (status <= 0) {
    snprintf(out.error, sizeof(out.error), "서버 연결 실패 (%s)",
             http.errorToString(status).c_str());
    http.end();
    return false;
  }
  if (status != 200) {
    String body = http.getString();
    http.end();
    bus::describeErrorBody(body.c_str(), body.length(), out.error, sizeof(out.error));
    if (strcmp(out.error, "응답을 해석할 수 없음") == 0) {
      snprintf(out.error, sizeof(out.error), "HTTP 오류 %d", status);
    }
    return false;
  }

  StreamByteReader reader(http.getStream());
  bool ok = weather::parseForecast(reader, target, out.day, out.error, sizeof(out.error));
  http.end();
  return ok;
}

// 초단기실황: 응답이 작아서 한 번에 받습니다.
static bool fetchNowcast(const struct tm& now, weather::Now& out, char* error, size_t errorSize) {
  int nx, ny;
  gridPosition(&nx, &ny);
  for (int back = 0; back < 2; back++) {  // 아직 발표 전이면 한 시간 전 것
    char baseDate[9], baseTime[5];
    weather::nowcastBase(now, back, baseDate, baseTime);
    static char url[512];
    if (!weather::buildNowcastUrl(SERVICE_KEY, nx, ny, baseDate, baseTime, url, sizeof(url))) {
      return false;
    }
    HTTPClient http;
    if (!httpBegin(http, url)) return false;
    int status = http.GET();
    if (status <= 0) {
      http.end();
      bus::copyUtf8(error, errorSize, "서버 연결 실패");
      return false;
    }
    String body = http.getString();
    http.end();
    if (weather::parseNowcast(body.c_str(), body.length(), out, error, errorSize)) return true;
    if (strstr(error, "아직 없음") == nullptr) return false;
  }
  return false;
}

bool fetchWeather(bool tomorrow, WeatherData& out) {
  out.ok = false;
  out.tomorrow = tomorrow;
  out.error[0] = '\0';
  out.now.ok = false;

  struct tm now;
  if (!platformLocalTime(&now)) {
    bus::copyUtf8(out.error, sizeof(out.error), "시계 맞추는 중...");
    return false;
  }

  if (!fetchForecast(now, tomorrow, out)) {
    Serial.printf("[날씨] 예보 실패: %s\n", out.error);
    return false;
  }

  if (!tomorrow) {
    char err[96];
    if (!fetchNowcast(now, out.now, err, sizeof(err))) {
      Serial.printf("[날씨] 현재 기온 실패 (예보만 표시): %s\n", err);
    }
  }

  out.ok = true;
  Serial.println("[날씨] 완료");
  return true;
}
