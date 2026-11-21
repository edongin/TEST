// 날씨 응답 해석과 화면 전환 규칙을 PC에서 확인하는 테스트입니다.
#include <stdio.h>
#include <string.h>

#include "app_logic.h"
#include "weather_api.h"

using namespace weather;

static int failures = 0;

#define CHECK(cond)                                            \
  do {                                                         \
    if (!(cond)) {                                             \
      printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                              \
    }                                                          \
  } while (0)

#define CHECK_STR(a, b)                                                         \
  do {                                                                          \
    if (strcmp((a), (b)) != 0) {                                                \
      printf("  FAIL %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
      failures++;                                                               \
    }                                                                           \
  } while (0)

static struct tm at(int y, int mon, int d, int h, int min) {
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year = y - 1900;
  t.tm_mon = mon - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = min;
  return t;
}

// ---------------------------------------------------------------- 격자, 날짜
static void testGrid() {
  puts("위도·경도 → 격자 좌표");
  int nx, ny;
  latLonToGrid(37.5665, 126.9780, &nx, &ny);  // 서울시청
  CHECK(nx == 60 && ny == 127);
  latLonToGrid(35.1796, 129.0756, &nx, &ny);  // 부산시청
  CHECK(nx == 98 && ny == 76);
}

static void testDates() {
  puts("발표 시각 고르기");
  char d[9], t[5];
  forecastBase(at(2026, 11, 17, 7, 30), true, d, t);
  CHECK_STR(d, "20261117");
  CHECK_STR(t, "0200");
  forecastBase(at(2026, 11, 17, 1, 0), true, d, t);  // 02시 발표 전이면 어제 23시
  CHECK_STR(d, "20261116");
  CHECK_STR(t, "2300");
  forecastBase(at(2026, 11, 17, 21, 5), false, d, t);
  CHECK_STR(d, "20261117");
  CHECK_STR(t, "2000");
  forecastBase(at(2026, 11, 17, 20, 5), false, d, t);  // 20시 발표는 20:10 부터
  CHECK_STR(t, "1700");
  forecastBase(at(2027, 1, 1, 0, 30), false, d, t);  // 해가 바뀌는 경우
  CHECK_STR(d, "20261231");
  CHECK_STR(t, "2300");

  nowcastBase(at(2026, 11, 17, 7, 10), 0, d, t);
  CHECK_STR(t, "0600");
  nowcastBase(at(2026, 11, 17, 7, 20), 0, d, t);
  CHECK_STR(t, "0700");
  nowcastBase(at(2026, 11, 17, 7, 20), 1, d, t);
  CHECK_STR(t, "0600");
  nowcastBase(at(2026, 11, 17, 0, 5), 0, d, t);
  CHECK_STR(d, "20261116");
  CHECK_STR(t, "2300");

  int wday;
  dateAfter(at(2026, 11, 17, 8, 0), 0, d, &wday);
  CHECK(wday == 2);  // 화요일
  dateAfter(at(2026, 12, 31, 8, 0), 1, d, &wday);
  CHECK_STR(d, "20270101");
  CHECK(wday == 5);  // 금요일
  dateAfter(at(2028, 2, 28, 8, 0), 1, d, nullptr);
  CHECK_STR(d, "20280229");  // 윤년
}

static void testUrl() {
  puts("날씨 요청 주소");
  char url[512];
  CHECK(buildForecastUrl("k+y", 60, 127, "20261117", "0200", url, sizeof(url)));
  CHECK_STR(url,
            "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getVilageFcst"
            "?serviceKey=k%2By&pageNo=1&numOfRows=1000&dataType=JSON"
            "&base_date=20261117&base_time=0200&nx=60&ny=127");
  CHECK(buildNowcastUrl("key", 60, 127, "20261117", "0700", url, sizeof(url)));
  CHECK(strstr(url, "/getUltraSrtNcst?") != nullptr);
  CHECK(!buildForecastUrl("", 60, 127, "20261117", "0200", url, sizeof(url)));
}

// ---------------------------------------------------------------- 단기예보
#define ITEM(date, time, cat, val)                                                   \
  "{\"baseDate\":\"20261117\",\"baseTime\":\"0200\",\"category\":\"" cat              \
  "\",\"fcstDate\":\"" date "\",\"fcstTime\":\"" time "\",\"fcstValue\":\"" val "\"," \
  "\"nx\":60,\"ny\":127}"

static const char* FORECAST =
    "{\"response\":{\"header\":{\"resultCode\":\"00\",\"resultMsg\":\"NORMAL_SERVICE\"},"
    "\"body\":{\"dataType\":\"JSON\",\"items\":{\"item\":["
    ITEM("20261117", "0600", "TMP", "-1") ","
    ITEM("20261117", "0600", "SKY", "1") ","
    ITEM("20261117", "0600", "PTY", "0") ","
    ITEM("20261117", "0600", "POP", "0") ","
    ITEM("20261117", "0600", "PCP", "강수없음") ","
    ITEM("20261117", "0600", "TMN", "-2.0") ","
    ITEM("20261117", "0900", "TMP", "2") ","
    ITEM("20261117", "0900", "SKY", "3") ","
    ITEM("20261117", "0900", "PTY", "0") ","
    ITEM("20261117", "0900", "POP", "20") ","
    ITEM("20261117", "1500", "TMP", "8") ","
    ITEM("20261117", "1500", "SKY", "4") ","
    ITEM("20261117", "1500", "PTY", "1") ","
    ITEM("20261117", "1500", "POP", "70") ","
    ITEM("20261117", "1500", "TMX", "9.0") ","
    ITEM("20261118", "0600", "TMP", "0") ","
    ITEM("20261118", "0600", "SKY", "4") ","
    ITEM("20261118", "0600", "PTY", "3") ","
    ITEM("20261118", "0600", "POP", "60") ","
    ITEM("20261118", "1200", "TMP", "4") ","
    ITEM("20261118", "1200", "SKY", "3") ","
    ITEM("20261118", "1200", "PTY", "0") ","
    ITEM("20261118", "1200", "POP", "30")
    "]},\"pageNo\":1,\"numOfRows\":1000,\"totalCount\":23}}}";

static void testForecastToday() {
  puts("단기예보: 오늘");
  StringReader r(FORECAST, strlen(FORECAST));
  Day day;
  char err[96];
  CHECK(parseForecast(r, "20261117", day, err, sizeof(err)));
  CHECK(day.ok);
  CHECK(day.tmin10 == -20);
  CHECK(day.tmax10 == 90);
  CHECK(day.hours[6].temp == -1 && day.hours[6].sky == 1 && day.hours[6].pop == 0);
  CHECK(day.hours[9].temp == 2 && day.hours[9].sky == 3 && day.hours[9].pop == 20);
  CHECK(day.hours[15].pty == 1 && day.hours[15].pop == 70);
  CHECK(day.hours[12].temp == UNKNOWN);

  int pop;
  CHECK(summarize(day, 6, 12, &pop) == COND_PARTLY);  // 맑음 1, 구름많음 1 → 더 흐린 쪽
  CHECK(pop == 20);
  CHECK(summarize(day, 12, 24, &pop) == COND_RAIN);
  CHECK(pop == 70);
  CHECK_STR(conditionName(COND_RAIN), "비");
}

static void testForecastTomorrow() {
  puts("단기예보: 내일 (최저/최고가 없으면 시간별 기온으로)");
  StringReader r(FORECAST, strlen(FORECAST));
  Day day;
  char err[96];
  CHECK(parseForecast(r, "20261118", day, err, sizeof(err)));
  CHECK(day.tmin10 == 0);
  CHECK(day.tmax10 == 40);
  CHECK(hourCondition(day.hours[6]) == COND_SNOW);
  int pop;
  CHECK(summarize(day, 0, 12, &pop) == COND_SNOW && pop == 60);
}

static void testForecastStopsEarly() {
  puts("단기예보: 원하는 날짜가 지나면 더 읽지 않음");
  // 내일 항목 뒤가 깨져 있어도 오늘 날씨는 문제없이 읽혀야 함
  const char* json =
      "{\"response\":{\"header\":{\"resultCode\":\"00\"},\"body\":{\"items\": {\"item\" : [ "
      ITEM("20261117", "0900", "TMP", "5") " , "
      ITEM("20261118", "0900", "TMP", "6") ", {\"broken";
  StringReader r(json, strlen(json));
  Day day;
  char err[96];
  CHECK(parseForecast(r, "20261117", day, err, sizeof(err)));
  CHECK(day.hours[9].temp == 5);
  CHECK(day.tmin10 == 50 && day.tmax10 == 50);
}

static void testForecastErrors() {
  puts("단기예보: 오류 응답");
  Day day;
  char err[96];
  const char* noData =
      "{\"response\":{\"header\":{\"resultCode\":\"03\",\"resultMsg\":\"NO_DATA\"}}}";
  StringReader r1(noData, strlen(noData));
  CHECK(!parseForecast(r1, "20261117", day, err, sizeof(err)));
  CHECK(strstr(err, "아직 없음") != nullptr);

  const char* other =
      "{\"response\":{\"header\":{\"resultCode\":\"10\",\"resultMsg\":\"INVALID_REQUEST_PARAMETER_ERROR\"}}}";
  StringReader r2(other, strlen(other));
  CHECK(!parseForecast(r2, "20261117", day, err, sizeof(err)));
  CHECK(strstr(err, "날씨 API 오류 10") != nullptr);

  const char* xml =
      "<OpenAPI_ServiceResponse><cmmMsgHeader><returnAuthMsg>SERVICE_KEY_IS_NOT_REGISTERED_ERROR"
      "</returnAuthMsg></cmmMsgHeader></OpenAPI_ServiceResponse>";
  StringReader r3(xml, strlen(xml));
  CHECK(!parseForecast(r3, "20261117", day, err, sizeof(err)));
  CHECK(strstr(err, "인증키 미등록") != nullptr);

  const char* cut = "{\"response\":{\"header\":{\"resultCode\":\"00\"},\"body\":{\"items\":{\"item\":[{\"cat";
  StringReader r4(cut, strlen(cut));
  CHECK(!parseForecast(r4, "20261117", day, err, sizeof(err)));
  CHECK(strstr(err, "끊김") != nullptr);

  StringReader r5(FORECAST, strlen(FORECAST));
  CHECK(!parseForecast(r5, "20261120", day, err, sizeof(err)));  // 응답에 없는 날짜
  CHECK(strstr(err, "없음") != nullptr);

  StringReader r6("", 0);
  CHECK(!parseForecast(r6, "20261117", day, err, sizeof(err)));
}

// ---------------------------------------------------------------- 초단기실황
static void testNowcast() {
  puts("초단기실황");
  const char* json =
      "{\"response\":{\"header\":{\"resultCode\":\"00\",\"resultMsg\":\"NORMAL_SERVICE\"},"
      "\"body\":{\"dataType\":\"JSON\",\"items\":{\"item\":["
      "{\"baseDate\":\"20261117\",\"baseTime\":\"0700\",\"category\":\"PTY\",\"nx\":60,\"ny\":127,\"obsrValue\":\"0\"},"
      "{\"baseDate\":\"20261117\",\"baseTime\":\"0700\",\"category\":\"REH\",\"nx\":60,\"ny\":127,\"obsrValue\":\"65\"},"
      "{\"baseDate\":\"20261117\",\"baseTime\":\"0700\",\"category\":\"RN1\",\"nx\":60,\"ny\":127,\"obsrValue\":\"0\"},"
      "{\"baseDate\":\"20261117\",\"baseTime\":\"0700\",\"category\":\"T1H\",\"nx\":60,\"ny\":127,\"obsrValue\":\"-3.2\"}"
      "]},\"pageNo\":1,\"numOfRows\":20,\"totalCount\":4}}}";
  Now now;
  char err[96];
  CHECK(parseNowcast(json, strlen(json), now, err, sizeof(err)));
  CHECK(now.temp10 == -32);
  CHECK(now.pty == 0 && now.humidity == 65);
  CHECK(nowCondition(now, 4) == COND_CLOUDY);
  now.pty = 7;
  CHECK(nowCondition(now, 1) == COND_SNOW);

  const char* bad = "{\"response\":{\"header\":{\"resultCode\":\"03\",\"resultMsg\":\"NO_DATA\"}}}";
  CHECK(!parseNowcast(bad, strlen(bad), now, err, sizeof(err)));
}

// ---------------------------------------------------------------- 화면 전환 규칙
static void testAppLogic() {
  puts("화면 전환 규칙");
  app::Schedule s = {6, 12, 20, 23};
  CHECK(app::homeModeAt(s, 7, true) == app::HOME_TODAY);
  CHECK(app::homeModeAt(s, 11, true) == app::HOME_TODAY);
  CHECK(app::homeModeAt(s, 12, true) == app::HOME_OFF);  // 오후는 꺼짐
  CHECK(app::homeModeAt(s, 19, true) == app::HOME_OFF);
  CHECK(app::homeModeAt(s, 20, true) == app::HOME_TOMORROW);
  CHECK(app::homeModeAt(s, 22, true) == app::HOME_TOMORROW);
  CHECK(app::homeModeAt(s, 23, true) == app::HOME_OFF);
  CHECK(app::homeModeAt(s, 3, true) == app::HOME_OFF);
  CHECK(app::homeModeAt(s, 15, false) == app::HOME_TODAY);  // 시간을 모르면 일단 날씨

  // 기본 → 정류장1 → 정류장2 → 정류장3 → 기본
  CHECK(app::nextScreen(-1, 3) == 0);
  CHECK(app::nextScreen(0, 3) == 1);
  CHECK(app::nextScreen(1, 3) == 2);
  CHECK(app::nextScreen(2, 3) == -1);
  CHECK(app::nextScreen(-1, 1) == 0);
  CHECK(app::nextScreen(0, 1) == -1);
  CHECK(app::nextScreen(-1, 0) == -1);
  // 기본 화면이 꺼져 있는 시간: 정류장 다음에 날씨(번호 = 정류장 수)
  CHECK(app::nextScreen(2, 3, true) == 3);
  CHECK(app::nextScreen(3, 3, true) == -1);
  CHECK(app::nextScreen(-1, 0, true) == 0);
  CHECK(app::touchWeatherAt(s, 15, true) == app::HOME_TODAY);
  CHECK(app::touchWeatherAt(s, 23, true) == app::HOME_TOMORROW);
  CHECK(app::touchWeatherAt(s, 3, true) == app::HOME_TODAY);

  CHECK(!app::stationTimedOut(29 * 60000UL, 0, 30));
  CHECK(app::stationTimedOut(30 * 60000UL, 0, 30));
  CHECK(!app::stationTimedOut(100, 0xFFFFFF00UL, 30));  // millis() 가 한 바퀴 돌아도 정상
}

int main() {
  testGrid();
  testDates();
  testUrl();
  testForecastToday();
  testForecastTomorrow();
  testForecastStopsEarly();
  testForecastErrors();
  testNowcast();
  testAppLogic();
  if (failures) {
    printf("\n%d개 실패\n", failures);
    return 1;
  }
  puts("\n모든 테스트 통과");
  return 0;
}
