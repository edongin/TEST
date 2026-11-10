// 버스 도착정보 API 주소 만들기 + 응답(JSON) 해석
// 이 파일과 bus_api.cpp 는 Arduino 기능을 쓰지 않아서 PC에서도 테스트할 수 있습니다.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace bus {

enum Provider {
  PROVIDER_GYEONGGI = 1,  // 경기도 버스정보 (공공데이터포털)
  PROVIDER_SEOUL = 2,     // 서울시 버스도착정보
  PROVIDER_TAGO = 3,      // 국토교통부 TAGO (전국 주요 도시)
};

constexpr int MAX_ROUTES = 30;

// 버스 한 대의 도착 예정 정보
struct Arrival {
  int32_t sec;    // 도착까지 남은 시간(초). -1 = 정보 없음
  int16_t stops;  // 남은 정류장 수. -1 = 모름
  int16_t seats;  // 빈자리 수. -1 = 모름
  char note[32];  // 시간 대신 보여줄 문구 (예: "운행종료", "출발대기")
};

// 노선 하나 (첫 번째 버스, 두 번째 버스)
struct Route {
  char name[20];  // 노선 번호 (예: "11-3")
  char dest[40];  // 방면 (예: "범계역 방면")
  Arrival next[2];
};

struct Result {
  bool ok;
  char error[96];
  char station[48];  // 정류장 이름 (API가 알려줄 때만)
  int count;
  Route routes[MAX_ROUTES];
};

// 공공데이터포털 인증키를 URL에 넣을 수 있게 바꿉니다.
// 이미 인코딩된 키(% 포함)는 그대로, 디코딩 키는 인코딩해서 씁니다.
void encodeServiceKey(const char* key, char* out, size_t outSize);

// API 요청 주소를 만듭니다. cityCode 는 TAGO 에서만 씁니다.
bool buildUrl(Provider provider, const char* serviceKey, const char* stationId,
              const char* cityCode, char* out, size_t outSize);

// 응답 본문을 해석해서 result 에 채웁니다. 실패하면 result.error 에 이유를 적습니다.
bool parseResponse(Provider provider, const char* body, size_t len, Result& result);

// 서울 도착 문구("3분12초후[2번째 전]")에서 초와 정류장 수를 꺼냅니다.
// 시간이 없으면 -1 을 돌려줍니다.
int32_t parseSeoulMessage(const char* msg, int16_t* stops);

// filter 는 "11-3, 5, 9" 처럼 쉼표로 구분. 비어 있으면 모든 노선을 보여줍니다.
bool routeAllowed(const char* filter, const char* routeName);

// 노선 필터를 적용합니다.
void applyRouteFilter(Result& result, const char* filter);

// 먼저 도착하는 노선이 위로 오게 정렬합니다. 정보 없는 노선은 맨 아래.
void sortByArrival(Result& result);

// UTF-8 글자가 중간에 잘리지 않게 문자열을 복사합니다.
void copyUtf8(char* dst, size_t dstSize, const char* src);

}  // namespace bus
