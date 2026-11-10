#include "bus_api.h"

#include <ArduinoJson.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace bus {

// ---------------------------------------------------------------------------
// 작은 도우미 함수들
// ---------------------------------------------------------------------------

void copyUtf8(char* dst, size_t dstSize, const char* src) {
  if (dstSize == 0) return;
  if (!src) src = "";
  size_t n = strlen(src);
  if (n >= dstSize) {
    n = dstSize - 1;
    // 잘리는 지점이 한 글자(멀티바이트)의 중간이면 그 글자 앞까지만 복사
    while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;
  }
  memcpy(dst, src, n);
  dst[n] = '\0';
}

static void setError(Result& r, const char* msg) {
  r.ok = false;
  copyUtf8(r.error, sizeof(r.error), msg);
}

// snprintf 와 같지만, 길면 한글이 깨지지 않게 글자 단위로 자릅니다.
static void formatUtf8(char* dst, size_t dstSize, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));
static void formatUtf8(char* dst, size_t dstSize, const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  copyUtf8(dst, dstSize, buf);
}

static void resetArrival(Arrival& a) {
  a.sec = -1;
  a.stops = -1;
  a.seats = -1;
  a.note[0] = '\0';
}

static void resetResult(Result& r) {
  r.ok = false;
  r.error[0] = '\0';
  r.station[0] = '\0';
  r.count = 0;
}

static Route* addRoute(Result& r) {
  if (r.count >= MAX_ROUTES) return nullptr;
  Route& route = r.routes[r.count++];
  route.name[0] = '\0';
  route.dest[0] = '\0';
  resetArrival(route.next[0]);
  resetArrival(route.next[1]);
  return &route;
}

// API 마다 숫자를 숫자로 주기도, 문자열("3", "")로 주기도 해서 둘 다 받습니다.
static long toInt(JsonVariantConst v, long fallback = -1) {
  if (v.isNull()) return fallback;
  if (v.is<long>()) return v.as<long>();
  if (v.is<double>()) return (long)v.as<double>();
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (!s || !*s) return fallback;
    char* end = nullptr;
    long n = strtol(s, &end, 10);
    return end == s ? fallback : n;
  }
  return fallback;
}

static void toText(JsonVariantConst v, char* dst, size_t dstSize) {
  if (v.is<const char*>()) {
    copyUtf8(dst, dstSize, v.as<const char*>());
  } else if (v.is<long>()) {
    snprintf(dst, dstSize, "%ld", v.as<long>());
  } else if (v.is<double>()) {
    snprintf(dst, dstSize, "%g", v.as<double>());
  } else if (dstSize) {
    dst[0] = '\0';
  }
}

// 결과가 1건이면 배열 대신 객체 하나만 오는 API가 있어서 둘 다 처리합니다.
template <typename Fn>
static void forEachItem(JsonVariantConst list, Fn fn) {
  if (list.is<JsonArrayConst>()) {
    for (JsonVariantConst item : list.as<JsonArrayConst>()) {
      if (item.is<JsonObjectConst>()) fn(item.as<JsonObjectConst>());
    }
  } else if (list.is<JsonObjectConst>()) {
    fn(list.as<JsonObjectConst>());
  }
}

static void setDest(Route& route, const char* dest) {
  if (!dest || !*dest) return;
  formatUtf8(route.dest, sizeof(route.dest), "%s 방면", dest);
}

static const char* skipSpaces(const char* s, size_t& len) {
  while (len > 0 && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) {
    s++;
    len--;
  }
  return s;
}

static bool containsN(const char* s, size_t len, const char* needle) {
  size_t n = strlen(needle);
  if (n > len) return false;
  for (size_t i = 0; i + n <= len; i++) {
    if (memcmp(s + i, needle, n) == 0) return true;
  }
  return false;
}

// <tag>값</tag> 의 값을 꺼냅니다. 공공데이터포털 오류는 XML로 오기 때문에 필요합니다.
static bool xmlTag(const char* s, size_t len, const char* tag, char* out, size_t outSize) {
  char open[48];
  snprintf(open, sizeof(open), "<%s>", tag);
  size_t n = strlen(open);
  for (size_t i = 0; i + n <= len; i++) {
    if (memcmp(s + i, open, n) != 0) continue;
    size_t start = i + n, end = start;
    while (end < len && s[end] != '<') end++;
    size_t m = end - start;
    char tmp[128];
    if (m >= sizeof(tmp)) m = sizeof(tmp) - 1;
    memcpy(tmp, s + start, m);
    tmp[m] = '\0';
    copyUtf8(out, outSize, tmp);
    return true;
  }
  return false;
}

// JSON 이 아닌 응답(인증키 오류 등)을 사람이 이해할 수 있는 문구로 바꿉니다.
static void describeNonJson(const char* body, size_t len, Result& r) {
  struct Known {
    const char* code;
    const char* message;
  };
  static const Known known[] = {
      {"SERVICE_KEY_IS_NOT_REGISTERED", "인증키 미등록 (승인 직후면 1~2시간 뒤 다시 시도)"},
      {"LIMITED_NUMBER_OF_SERVICE_REQUESTS", "오늘 호출 한도 초과 (갱신 간격을 늘리세요)"},
      {"SERVICE_ACCESS_DENIED", "이 API 활용신청이 안 되어 있음"},
      {"DEADLINE_HAS_EXPIRED", "API 활용기간 만료"},
      {"UNREGISTERED_IP", "등록되지 않은 IP"},
      {"Unauthorized", "인증키 오류 (Encoding 키인지 확인)"},
  };
  for (const Known& k : known) {
    if (containsN(body, len, k.code)) {
      setError(r, k.message);
      return;
    }
  }
  char msg[64] = "";
  if (xmlTag(body, len, "returnAuthMsg", msg, sizeof(msg)) ||
      xmlTag(body, len, "errMsg", msg, sizeof(msg)) ||
      xmlTag(body, len, "headerMsg", msg, sizeof(msg)) ||
      xmlTag(body, len, "resultMsg", msg, sizeof(msg))) {
    r.ok = false;
    formatUtf8(r.error, sizeof(r.error), "API 오류: %s", msg);
    return;
  }
  setError(r, "응답을 해석할 수 없음");
}

// ---------------------------------------------------------------------------
// 인증키, 요청 주소
// ---------------------------------------------------------------------------

void encodeServiceKey(const char* key, char* out, size_t outSize) {
  if (outSize == 0) return;
  if (strchr(key, '%')) {  // 이미 인코딩된 키
    copyUtf8(out, outSize, key);
    return;
  }
  static const char hex[] = "0123456789ABCDEF";
  size_t o = 0;
  for (const unsigned char* p = (const unsigned char*)key; *p; p++) {
    bool plain = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                 (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.' || *p == '~';
    size_t need = plain ? 1 : 3;
    if (o + need >= outSize) break;
    if (plain) {
      out[o++] = (char)*p;
    } else {
      out[o++] = '%';
      out[o++] = hex[*p >> 4];
      out[o++] = hex[*p & 0x0F];
    }
  }
  out[o] = '\0';
}

bool buildUrl(Provider provider, const char* serviceKey, const char* stationId,
              const char* cityCode, char* out, size_t outSize) {
  if (!serviceKey || !*serviceKey || !stationId || !*stationId) return false;
  char key[256];
  encodeServiceKey(serviceKey, key, sizeof(key));

  int n = -1;
  switch (provider) {
    case PROVIDER_GYEONGGI:
      n = snprintf(out, outSize,
                   "https://apis.data.go.kr/6410000/busarrivalservice/v2/getBusArrivalListv2"
                   "?serviceKey=%s&stationId=%s&format=json",
                   key, stationId);
      break;
    case PROVIDER_SEOUL:
      n = snprintf(out, outSize,
                   "http://ws.bus.go.kr/api/rest/stationinfo/getStationByUid"
                   "?serviceKey=%s&arsId=%s&resultType=json",
                   key, stationId);
      break;
    case PROVIDER_TAGO:
      if (!cityCode || !*cityCode) return false;
      n = snprintf(out, outSize,
                   "https://apis.data.go.kr/1613000/ArvlInfoInqireService/"
                   "getSttnAcctoArvlPrearngeInfoList"
                   "?serviceKey=%s&cityCode=%s&nodeId=%s&numOfRows=100&pageNo=1&_type=json",
                   key, cityCode, stationId);
      break;
  }
  return n > 0 && (size_t)n < outSize;
}

// ---------------------------------------------------------------------------
// 경기도: response.msgBody.busArrivalList[]
// ---------------------------------------------------------------------------

static bool parseGyeonggi(JsonDocument& doc, Result& r) {
  JsonObjectConst resp = doc["response"];
  if (resp.isNull()) {
    setError(r, "응답 형식이 예상과 다름");
    return false;
  }

  long code = toInt(resp["msgHeader"]["resultCode"], 0);
  if (code == 4) {  // 결과 없음: 지금 도착 예정인 버스가 없음
    r.ok = true;
    return true;
  }
  if (code != 0) {
    const char* msg = resp["msgHeader"]["resultMessage"] | "알 수 없는 오류";
    r.ok = false;
    formatUtf8(r.error, sizeof(r.error), "API 오류 %ld: %s", code, msg);
    return false;
  }

  forEachItem(resp["msgBody"]["busArrivalList"], [&](JsonObjectConst item) {
    Route* route = addRoute(r);
    if (!route) return;
    toText(item["routeName"], route->name, sizeof(route->name));
    setDest(*route, item["routeDestName"] | "");

    static const char* const secKeys[2] = {"predictTimeSec1", "predictTimeSec2"};
    static const char* const minKeys[2] = {"predictTime1", "predictTime2"};
    static const char* const stopKeys[2] = {"locationNo1", "locationNo2"};
    static const char* const seatKeys[2] = {"remainSeatCnt1", "remainSeatCnt2"};
    for (int i = 0; i < 2; i++) {
      Arrival& a = route->next[i];
      long sec = toInt(item[secKeys[i]]);
      if (sec < 0) {
        long min = toInt(item[minKeys[i]]);
        if (min >= 0) sec = min * 60;
      }
      a.sec = (int32_t)sec;
      if (sec >= 0) {
        a.stops = (int16_t)toInt(item[stopKeys[i]]);
        long seats = toInt(item[seatKeys[i]]);
        a.seats = (int16_t)(seats >= 0 ? seats : -1);
      }
    }
  });
  r.ok = true;
  return true;
}

// ---------------------------------------------------------------------------
// 서울: msgBody.itemList[] , 도착 정보는 "3분12초후[2번째 전]" 같은 문구로 옴
// ---------------------------------------------------------------------------

static bool startsWith(const char* s, const char* prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

int32_t parseSeoulMessage(const char* msg, int16_t* stops) {
  if (stops) *stops = -1;
  if (!msg) return -1;

  long minutes = -1, seconds = -1;
  for (const char* p = msg; *p;) {
    if (*p >= '0' && *p <= '9') {
      char* end = nullptr;
      long n = strtol(p, &end, 10);
      p = end;
      if (startsWith(p, "분")) {
        minutes = n;
      } else if (startsWith(p, "초")) {
        seconds = n;
      } else if (startsWith(p, "번째") && stops) {
        *stops = (int16_t)n;
      }
    } else {
      p++;
    }
  }
  if (minutes >= 0 || seconds >= 0) {
    return (int32_t)((minutes > 0 ? minutes : 0) * 60 + (seconds > 0 ? seconds : 0));
  }
  if (strstr(msg, "곧 도착")) return 0;
  return -1;
}

static bool parseSeoul(JsonDocument& doc, Result& r) {
  JsonObjectConst header = doc["msgHeader"];
  if (header.isNull()) {
    setError(r, "응답 형식이 예상과 다름");
    return false;
  }
  long code = toInt(header["headerCd"], 0);
  if (code == 4) {
    r.ok = true;
    return true;
  }
  if (code != 0) {
    const char* msg = header["headerMsg"] | "알 수 없는 오류";
    r.ok = false;
    formatUtf8(r.error, sizeof(r.error), "API 오류 %ld: %s", code, msg);
    return false;
  }

  forEachItem(doc["msgBody"]["itemList"], [&](JsonObjectConst item) {
    if (!r.station[0]) copyUtf8(r.station, sizeof(r.station), item["stNm"] | "");
    Route* route = addRoute(r);
    if (!route) return;
    toText(item["rtNm"], route->name, sizeof(route->name));
    setDest(*route, item["adirection"] | "");

    static const char* const msgKeys[2] = {"arrmsg1", "arrmsg2"};
    for (int i = 0; i < 2; i++) {
      Arrival& a = route->next[i];
      const char* msg = item[msgKeys[i]] | "";
      a.sec = parseSeoulMessage(msg, &a.stops);
      if (a.sec < 0) copyUtf8(a.note, sizeof(a.note), msg);  // 운행종료, 출발대기 등
    }
  });
  r.ok = true;
  return true;
}

// ---------------------------------------------------------------------------
// TAGO: response.body.items.item[] , 버스 한 대가 한 줄이라 노선별로 묶습니다
// ---------------------------------------------------------------------------

static bool parseTago(JsonDocument& doc, Result& r) {
  JsonObjectConst resp = doc["response"];
  if (resp.isNull()) {
    setError(r, "응답 형식이 예상과 다름");
    return false;
  }
  const char* code = resp["header"]["resultCode"] | "00";
  if (strcmp(code, "00") != 0) {
    const char* msg = resp["header"]["resultMsg"] | "알 수 없는 오류";
    r.ok = false;
    formatUtf8(r.error, sizeof(r.error), "API 오류 %s: %s", code, msg);
    return false;
  }

  // 결과가 없으면 items 가 "" (빈 문자열)로 옵니다. forEachItem 이 알아서 건너뜁니다.
  forEachItem(resp["body"]["items"]["item"], [&](JsonObjectConst item) {
    if (!r.station[0]) copyUtf8(r.station, sizeof(r.station), item["nodenm"] | "");

    char name[sizeof(Route::name)];
    toText(item["routeno"], name, sizeof(name));
    Arrival bus;
    resetArrival(bus);
    bus.sec = (int32_t)toInt(item["arrtime"]);
    bus.stops = (int16_t)toInt(item["arrprevstationcnt"]);
    if (bus.sec < 0) return;

    Route* route = nullptr;
    for (int i = 0; i < r.count; i++) {
      if (strcmp(r.routes[i].name, name) == 0) route = &r.routes[i];
    }
    if (!route) {
      route = addRoute(r);
      if (!route) return;
      copyUtf8(route->name, sizeof(route->name), name);
    }

    // 빨리 오는 두 대만 남깁니다.
    Arrival* a = route->next;
    if (a[0].sec < 0 || bus.sec < a[0].sec) {
      a[1] = a[0];
      a[0] = bus;
    } else if (a[1].sec < 0 || bus.sec < a[1].sec) {
      a[1] = bus;
    }
  });
  r.ok = true;
  return true;
}

// ---------------------------------------------------------------------------

bool parseResponse(Provider provider, const char* body, size_t len, Result& r) {
  resetResult(r);
  if (!body) body = "";
  const char* start = skipSpaces(body, len);
  if (len == 0) {
    setError(r, "빈 응답");
    return false;
  }
  if (*start != '{') {
    describeNonJson(start, len, r);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, start, len);
  if (err) {
    r.ok = false;
    formatUtf8(r.error, sizeof(r.error), "JSON 해석 실패: %s", err.c_str());
    return false;
  }

  switch (provider) {
    case PROVIDER_GYEONGGI:
      return parseGyeonggi(doc, r);
    case PROVIDER_SEOUL:
      return parseSeoul(doc, r);
    case PROVIDER_TAGO:
      return parseTago(doc, r);
  }
  setError(r, "알 수 없는 API 종류");
  return false;
}

// ---------------------------------------------------------------------------
// 노선 필터, 정렬
// ---------------------------------------------------------------------------

bool routeAllowed(const char* filter, const char* routeName) {
  if (!filter || !*filter) return true;
  size_t nameLen = strlen(routeName);
  const char* p = filter;
  while (*p) {
    while (*p == ' ' || *p == ',') p++;
    const char* start = p;
    while (*p && *p != ',') p++;
    const char* end = p;
    while (end > start && end[-1] == ' ') end--;
    size_t n = (size_t)(end - start);
    if (n > 0 && n == nameLen && memcmp(start, routeName, n) == 0) return true;
  }
  return false;
}

void applyRouteFilter(Result& r, const char* filter) {
  if (!filter || !*filter) return;
  int kept = 0;
  for (int i = 0; i < r.count; i++) {
    if (routeAllowed(filter, r.routes[i].name)) {
      if (kept != i) r.routes[kept] = r.routes[i];
      kept++;
    }
  }
  r.count = kept;
}

static int32_t sortKey(const Route& route) {
  return route.next[0].sec >= 0 ? route.next[0].sec : INT32_MAX;
}

void sortByArrival(Result& r) {
  // 노선 수가 적어서 단순한 삽입 정렬로 충분합니다. 같은 시간이면 원래 순서 유지.
  for (int i = 1; i < r.count; i++) {
    Route tmp = r.routes[i];
    int32_t key = sortKey(tmp);
    int j = i - 1;
    while (j >= 0 && sortKey(r.routes[j]) > key) {
      r.routes[j + 1] = r.routes[j];
      j--;
    }
    r.routes[j + 1] = tmp;
  }
}

}  // namespace bus
