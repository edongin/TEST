// PC에서 API 응답 해석 코드를 확인하는 테스트입니다. (ESP32 없이 실행)
//   cd test_native && make
#include <stdio.h>
#include <string.h>

#include "arrival_text.h"
#include "bus_api.h"

using namespace bus;

static int failures = 0;

#define CHECK(cond)                                                \
  do {                                                             \
    if (!(cond)) {                                                 \
      printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
      failures++;                                                  \
    }                                                              \
  } while (0)

#define CHECK_STR(a, b)                                                            \
  do {                                                                             \
    if (strcmp((a), (b)) != 0) {                                                   \
      printf("  FAIL %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b));    \
      failures++;                                                                  \
    }                                                                              \
  } while (0)

static Result result;

static bool parse(Provider p, const char* json) {
  return parseResponse(p, json, strlen(json), result);
}

// ---------------------------------------------------------------- 경기도
static void testGyeonggi() {
  puts("경기도 응답");
  const char* json = R"({"response":{
    "comMsgHeader":{"errMsg":null,"returnCode":null},
    "msgHeader":{"queryTime":"2026-11-10 14:02:11.123","resultCode":0,"resultMessage":"정상적으로 처리되었습니다."},
    "msgBody":{"busArrivalList":[
      {"routeName":"11-3","routeDestName":"범계역","predictTime1":7,"predictTimeSec1":412,
       "locationNo1":4,"remainSeatCnt1":-1,"predictTime2":21,"predictTimeSec2":1260,
       "locationNo2":12,"remainSeatCnt2":-1,"flag":"PASS"},
      {"routeName":3330,"routeDestName":"강남역","predictTime1":2,"predictTimeSec1":"",
       "locationNo1":1,"remainSeatCnt1":23,"predictTime2":"","predictTimeSec2":"",
       "locationNo2":"","remainSeatCnt2":"","flag":"PASS"},
      {"routeName":"5","predictTime1":"","predictTime2":"","flag":"WAIT"}
    ]}}})";
  CHECK(parse(PROVIDER_GYEONGGI, json));
  CHECK(result.ok);
  CHECK(result.count == 3);

  const Route& a = result.routes[0];
  CHECK_STR(a.name, "11-3");
  CHECK_STR(a.dest, "범계역 방면");
  CHECK(a.next[0].sec == 412);
  CHECK(a.next[0].stops == 4);
  CHECK(a.next[0].seats == -1);
  CHECK(a.next[1].sec == 1260);
  CHECK(a.next[1].stops == 12);

  const Route& b = result.routes[1];
  CHECK_STR(b.name, "3330");          // 숫자로 와도 문자열로 바뀌어야 함
  CHECK(b.next[0].sec == 120);        // 초가 비어 있으면 분 * 60
  CHECK(b.next[0].seats == 23);
  CHECK(b.next[1].sec == -1);         // 두 번째 버스 없음

  const Route& c = result.routes[2];
  CHECK(c.next[0].sec == -1);
  CHECK_STR(c.dest, "");

  sortByArrival(result);
  CHECK_STR(result.routes[0].name, "3330");
  CHECK_STR(result.routes[1].name, "11-3");
  CHECK_STR(result.routes[2].name, "5");  // 정보 없는 노선은 맨 아래
}

static void testGyeonggiSingleObject() {
  puts("경기도: 결과가 1건이라 배열 대신 객체로 올 때");
  const char* json = R"({"response":{"msgHeader":{"resultCode":0},
    "msgBody":{"busArrivalList":{"routeName":"9","predictTimeSec1":59,"locationNo1":1}}}})";
  CHECK(parse(PROVIDER_GYEONGGI, json));
  CHECK(result.count == 1);
  CHECK_STR(result.routes[0].name, "9");
  CHECK(result.routes[0].next[0].sec == 59);
}

static void testGyeonggiNoResult() {
  puts("경기도: 도착 예정 버스 없음 (resultCode 4)");
  const char* json = R"({"response":{"msgHeader":{"resultCode":4,"resultMessage":"결과가 존재하지 않습니다."},"msgBody":null}})";
  CHECK(parse(PROVIDER_GYEONGGI, json));
  CHECK(result.ok);
  CHECK(result.count == 0);
}

static void testGyeonggiApiError() {
  puts("경기도: API 오류 코드");
  const char* json = R"({"response":{"msgHeader":{"resultCode":"2","resultMessage":"필수 요청 Parameter 가 존재하지 않습니다."}}})";
  CHECK(!parse(PROVIDER_GYEONGGI, json));
  CHECK(strstr(result.error, "API 오류 2") != nullptr);
}

// ---------------------------------------------------------------- 서울
static void testSeoul() {
  puts("서울 응답");
  const char* json = R"({"comMsgHeader":{},"msgHeader":{"headerCd":"0","headerMsg":"정상적으로 처리되었습니다.","itemCount":0},
    "msgBody":{"itemList":[
      {"rtNm":"470","stNm":"광화문","adirection":"상암차고지","arrmsg1":"3분12초후[2번째 전]","arrmsg2":"14분후[7번째 전]"},
      {"rtNm":"N37","stNm":"광화문","adirection":"송파공영차고지","arrmsg1":"운행종료","arrmsg2":"운행종료"},
      {"rtNm":"종로09","stNm":"광화문","adirection":"","arrmsg1":"곧 도착","arrmsg2":"출발대기"}
    ]}})";
  CHECK(parse(PROVIDER_SEOUL, json));
  CHECK(result.count == 3);
  CHECK_STR(result.station, "광화문");

  const Route& a = result.routes[0];
  CHECK_STR(a.name, "470");
  CHECK_STR(a.dest, "상암차고지 방면");
  CHECK(a.next[0].sec == 192);
  CHECK(a.next[0].stops == 2);
  CHECK(a.next[1].sec == 840);
  CHECK(a.next[1].stops == 7);

  const Route& b = result.routes[1];
  CHECK(b.next[0].sec == -1);
  CHECK_STR(b.next[0].note, "운행종료");

  const Route& c = result.routes[2];
  CHECK_STR(c.name, "종로09");
  CHECK(c.next[0].sec == 0);
  CHECK_STR(c.next[1].note, "출발대기");
}

static void testSeoulMessages() {
  puts("서울 도착 문구 해석");
  int16_t stops;
  CHECK(parseSeoulMessage("8분37초후[4번째 전]", &stops) == 517 && stops == 4);
  CHECK(parseSeoulMessage("45초후[1번째 전]", &stops) == 45 && stops == 1);
  CHECK(parseSeoulMessage("[막차] 5분후[3번째 전]", &stops) == 300 && stops == 3);
  CHECK(parseSeoulMessage("곧 도착", &stops) == 0);
  CHECK(parseSeoulMessage("회차대기", &stops) == -1 && stops == -1);
  CHECK(parseSeoulMessage("", &stops) == -1);
}

// ---------------------------------------------------------------- TAGO
static void testTago() {
  puts("TAGO 응답 (버스 한 대씩 와서 노선별로 묶기)");
  const char* json = R"({"response":{"header":{"resultCode":"00","resultMsg":"NORMAL SERVICE."},
    "body":{"items":{"item":[
      {"arrprevstationcnt":9,"arrtime":805,"nodeid":"DJB8001793","nodenm":"대전역","routeid":"DJB30300002","routeno":2,"routetp":"간선버스","vehicletp":"일반차량"},
      {"arrprevstationcnt":3,"arrtime":245,"nodenm":"대전역","routeno":2},
      {"arrprevstationcnt":15,"arrtime":1500,"nodenm":"대전역","routeno":2},
      {"arrprevstationcnt":1,"arrtime":60,"nodenm":"대전역","routeno":"급행1"}
    ]},"numOfRows":100,"pageNo":1,"totalCount":4}}})";
  CHECK(parse(PROVIDER_TAGO, json));
  CHECK(result.count == 2);
  CHECK_STR(result.station, "대전역");

  const Route& a = result.routes[0];
  CHECK_STR(a.name, "2");
  CHECK(a.next[0].sec == 245);   // 가장 빨리 오는 버스
  CHECK(a.next[0].stops == 3);
  CHECK(a.next[1].sec == 805);   // 그다음 버스 (1500초짜리는 버림)
  CHECK(a.next[1].stops == 9);
  CHECK_STR(result.routes[1].name, "급행1");
}

static void testTagoEmpty() {
  puts("TAGO: 결과 없음 (items 가 빈 문자열)");
  const char* json = R"({"response":{"header":{"resultCode":"00","resultMsg":"NORMAL SERVICE."},
    "body":{"items":"","numOfRows":100,"pageNo":1,"totalCount":0}}})";
  CHECK(parse(PROVIDER_TAGO, json));
  CHECK(result.ok);
  CHECK(result.count == 0);
}

// ---------------------------------------------------------------- 오류 응답
static void testGatewayErrors() {
  puts("공공데이터포털 인증키 오류 (XML 응답)");
  const char* xml =
      "<OpenAPI_ServiceResponse><cmmMsgHeader><errMsg>SERVICE ERROR</errMsg>"
      "<returnAuthMsg>SERVICE_KEY_IS_NOT_REGISTERED_ERROR</returnAuthMsg>"
      "<returnReasonCode>30</returnReasonCode></cmmMsgHeader></OpenAPI_ServiceResponse>";
  CHECK(!parse(PROVIDER_GYEONGGI, xml));
  CHECK(strstr(result.error, "인증키 미등록") != nullptr);

  CHECK(!parse(PROVIDER_TAGO, "Unauthorized"));
  CHECK(strstr(result.error, "인증키 오류") != nullptr);

  CHECK(!parse(PROVIDER_GYEONGGI, "<x><errMsg>SOMETHING</errMsg></x>"));
  CHECK_STR(result.error, "API 오류: SOMETHING");

  CHECK(!parse(PROVIDER_SEOUL, ""));
  CHECK(!parse(PROVIDER_SEOUL, "{\"msgHeader\":"));
  CHECK(strstr(result.error, "JSON 해석 실패") != nullptr);
}

// ---------------------------------------------------------------- 기타
static void testUrlAndKey() {
  puts("요청 주소와 인증키 인코딩");
  char key[128];
  encodeServiceKey("ab+c/d==", key, sizeof(key));
  CHECK_STR(key, "ab%2Bc%2Fd%3D%3D");
  encodeServiceKey("ab%2Bc", key, sizeof(key));  // 이미 인코딩된 키는 그대로
  CHECK_STR(key, "ab%2Bc");

  char url[512];
  CHECK(buildUrl(PROVIDER_GYEONGGI, "k+y", "208000123", "", url, sizeof(url)));
  CHECK_STR(url,
            "https://apis.data.go.kr/6410000/busarrivalservice/v2/getBusArrivalListv2"
            "?serviceKey=k%2By&stationId=208000123&format=json");
  CHECK(buildUrl(PROVIDER_SEOUL, "key", "01234", "", url, sizeof(url)));
  CHECK(strstr(url, "arsId=01234&resultType=json") != nullptr);
  CHECK(!buildUrl(PROVIDER_TAGO, "key", "DJB8001793", "", url, sizeof(url)));  // 도시코드 필요
  CHECK(buildUrl(PROVIDER_TAGO, "key", "DJB8001793", "25", url, sizeof(url)));
  CHECK(strstr(url, "cityCode=25&nodeId=DJB8001793") != nullptr);
  CHECK(!buildUrl(PROVIDER_GYEONGGI, "", "1", "", url, sizeof(url)));
  CHECK(!buildUrl(PROVIDER_GYEONGGI, "key", "1", "", url, 20));  // 버퍼가 너무 작음
}

static void testFilter() {
  puts("노선 필터");
  CHECK(routeAllowed("", "11-3"));
  CHECK(routeAllowed("11-3, 5 ,9", "5"));
  CHECK(routeAllowed("11-3, 5 ,9", "11-3"));
  CHECK(routeAllowed("11-3, 5 ,9", "9"));
  CHECK(!routeAllowed("11-3, 5 ,9", "11"));
  CHECK(!routeAllowed("11-3, 5 ,9", "59"));

  const char* json = R"({"response":{"msgHeader":{"resultCode":0},"msgBody":{"busArrivalList":[
    {"routeName":"1"},{"routeName":"2"},{"routeName":"3"}]}}})";
  CHECK(parse(PROVIDER_GYEONGGI, json));
  applyRouteFilter(result, "3,1");
  CHECK(result.count == 2);
  CHECK_STR(result.routes[0].name, "1");
  CHECK_STR(result.routes[1].name, "3");
}

static void testUtf8Copy() {
  puts("한글 자르기");
  char buf[8];
  copyUtf8(buf, sizeof(buf), "가나다라");  // 한 글자 3바이트 → 2글자(6바이트)까지만
  CHECK_STR(buf, "가나");
  copyUtf8(buf, sizeof(buf), "ab가나다");
  CHECK_STR(buf, "ab가");  // "ab가나"는 8바이트라 7바이트 칸에 안 들어감
  copyUtf8(buf, sizeof(buf), "abc");
  CHECK_STR(buf, "abc");
}

// ---------------------------------------------------------------- 화면 문구
static Arrival make(int32_t sec, int16_t stops = -1, int16_t seats = -1, const char* note = "") {
  Arrival a;
  a.sec = sec;
  a.stops = stops;
  a.seats = seats;
  copyUtf8(a.note, sizeof(a.note), note);
  return a;
}

static void testArrivalText() {
  puts("화면 문구");
  char buf[64];
  CHECK(formatFirst(make(412, 4), 0, buf, sizeof(buf)) == URGENCY_NORMAL);
  CHECK_STR(buf, "6분 (4정거장 전)");
  // 100초가 지나면 312초 → 5분, 3분 이내가 아님
  CHECK(formatFirst(make(412, 4), 100, buf, sizeof(buf)) == URGENCY_NORMAL);
  CHECK_STR(buf, "5분 (4정거장 전)");
  CHECK(formatFirst(make(170, 1, 23), 0, buf, sizeof(buf)) == URGENCY_SOON);
  CHECK_STR(buf, "2분 (1정거장 전) 23석");
  CHECK(formatFirst(make(50), 0, buf, sizeof(buf)) == URGENCY_SOON);
  CHECK_STR(buf, "곧 도착");
  CHECK(formatFirst(make(100), 500, buf, sizeof(buf)) == URGENCY_SOON);  // 시간이 지나도 음수가 되지 않음
  CHECK_STR(buf, "곧 도착");
  CHECK(formatFirst(make(-1), 0, buf, sizeof(buf)) == URGENCY_NONE);
  CHECK_STR(buf, "도착 정보 없음");
  CHECK(formatFirst(make(-1, -1, -1, "운행종료"), 0, buf, sizeof(buf)) == URGENCY_NONE);
  CHECK_STR(buf, "운행종료");

  formatSecond(make(1260, 12), 0, buf, sizeof(buf));
  CHECK_STR(buf, "다음 21분");
  formatSecond(make(-1, -1, -1, "출발대기"), 0, buf, sizeof(buf));
  CHECK_STR(buf, "다음 출발대기");
  formatSecond(make(-1), 0, buf, sizeof(buf));
  CHECK_STR(buf, "");
}

int main() {
  testGyeonggi();
  testGyeonggiSingleObject();
  testGyeonggiNoResult();
  testGyeonggiApiError();
  testSeoul();
  testSeoulMessages();
  testTago();
  testTagoEmpty();
  testGatewayErrors();
  testUrlAndKey();
  testFilter();
  testUtf8Copy();
  testArrivalText();

  if (failures) {
    printf("\n%d개 실패\n", failures);
    return 1;
  }
  puts("\n모든 테스트 통과");
  return 0;
}
