#include "bus_fetch.h"

#include <Arduino.h>
#include <HTTPClient.h>

#include "app_config.h"
#include "net_util.h"

bool fetchArrivals(const StationConfig& station, bus::Result& out) {
  static char url[512];
  const bus::Provider provider = (bus::Provider)BUS_PROVIDER;

  out.ok = false;
  out.count = 0;
  out.station[0] = '\0';

  if (!bus::buildUrl(provider, SERVICE_KEY, station.id, TAGO_CITY_CODE, url, sizeof(url))) {
    bus::copyUtf8(out.error, sizeof(out.error), "설정 확인: 인증키, 정류장 ID, 도시 코드");
    return false;
  }

  HTTPClient http;
  int status = 0;
  // 공공데이터포털 서버는 가끔 연결을 받아 주지 않아서, 연결 자체가 실패하면 한 번 더 시도합니다.
  for (int attempt = 1; attempt <= 2; attempt++) {
    if (!httpBegin(http, url)) {
      bus::copyUtf8(out.error, sizeof(out.error), "요청 주소 오류");
      return false;
    }
    Serial.printf("[버스] %s (%s) 정보 요청%s\n", station.label, station.id,
                  attempt > 1 ? " (다시 시도)" : "");
    status = http.GET();
    if (status > 0) break;
    snprintf(out.error, sizeof(out.error), "서버 연결 실패 (%s)",
             http.errorToString(status).c_str());
    http.end();
    Serial.printf("[버스] %s\n", out.error);
    if (attempt == 2) return false;
    delay(500);
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

  bus::applyRouteFilter(out, station.routeFilter);
  if (SORT_BY_ARRIVAL) bus::sortByArrival(out);
  Serial.printf("[버스] 노선 %d개\n", out.count);
  return true;
}
