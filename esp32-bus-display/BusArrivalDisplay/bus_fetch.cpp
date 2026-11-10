#include "bus_fetch.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#if !defined(ARDUINO_ARCH_RP2040)
#include <WiFiClientSecure.h>
#endif

#include "app_config.h"

bool fetchArrivals(bus::Result& out) {
  static char url[512];
  const bus::Provider provider = (bus::Provider)BUS_PROVIDER;

  out.ok = false;
  out.count = 0;
  out.station[0] = '\0';

  if (!bus::buildUrl(provider, SERVICE_KEY, STATION_ID, TAGO_CITY_CODE, url, sizeof(url))) {
    bus::copyUtf8(out.error, sizeof(out.error), "설정 확인: 인증키, 정류장 ID, 도시 코드");
    return false;
  }

  HTTPClient http;
  http.setTimeout(10000);
  bool started;

#if defined(ARDUINO_ARCH_RP2040)
  // Pico W: HTTPClient 가 https 연결을 직접 처리합니다.
  // 공개된 버스 정보라서 서버 인증서 검사는 생략합니다 (인증서 갱신 때 멈추지 않도록).
  // (setInsecure 는 https 연결을 만들어 버리므로 https 주소일 때만 부릅니다)
  if (strncmp(url, "https://", 8) == 0) http.setInsecure();
  started = http.begin(String(url));
#else
  // ESP32: 아래 두 클라이언트는 http 보다 오래 살아 있어야 해서 static 으로 둡니다.
  static WiFiClientSecure secureClient;
  static WiFiClient plainClient;
  http.setConnectTimeout(8000);
  if (strncmp(url, "https://", 8) == 0) {
    secureClient.setInsecure();  // 인증서 검사 생략 (위와 같은 이유)
    started = http.begin(secureClient, url);
  } else {
    started = http.begin(plainClient, url);
  }
#endif
  if (!started) {
    bus::copyUtf8(out.error, sizeof(out.error), "요청 주소 오류");
    return false;
  }

  Serial.printf("[버스] 정류장 %s 정보 요청\n", STATION_ID);
  int status = http.GET();
  if (status <= 0) {
    snprintf(out.error, sizeof(out.error), "서버 연결 실패 (%s)",
             http.errorToString(status).c_str());
    http.end();
    Serial.printf("[버스] %s\n", out.error);
    return false;
  }

  String body = http.getString();
  http.end();
  Serial.printf("[버스] HTTP %d, %u바이트\n", status, (unsigned)body.length());

  bool ok = bus::parseResponse(provider, body.c_str(), body.length(), out);
  if (status != 200) {
    // 오류 응답 안에 이유가 적혀 있으면 그걸 쓰고, 없으면 HTTP 상태 코드를 보여줍니다.
    if (ok || strcmp(out.error, "응답을 해석할 수 없음") == 0 || out.error[0] == '\0') {
      snprintf(out.error, sizeof(out.error), "HTTP 오류 %d", status);
    }
    out.ok = false;
    ok = false;
  }

  if (!ok) {
    Serial.printf("[버스] 실패: %s\n", out.error);
    if (body.length() > 0) Serial.printf("[버스] 응답 앞부분: %.200s\n", body.c_str());
    return false;
  }

  bus::applyRouteFilter(out, ROUTE_FILTER);
  if (SORT_BY_ARRIVAL) bus::sortByArrival(out);
  Serial.printf("[버스] 노선 %d개\n", out.count);
  return true;
}
