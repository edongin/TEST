#include "weather_api.h"

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bus_api.h"  // encodeServiceKey, copyUtf8, describeErrorBody

namespace weather {

const char* conditionName(Condition c) {
  switch (c) {
    case COND_CLEAR: return "맑음";
    case COND_PARTLY: return "구름많음";
    case COND_CLOUDY: return "흐림";
    case COND_RAIN: return "비";
    case COND_RAIN_SNOW: return "비/눈";
    case COND_SNOW: return "눈";
    case COND_SHOWER: return "소나기";
    default: return "정보 없음";
  }
}

// ---------------------------------------------------------------------------
// 격자 좌표 (기상청 공식 람베르트 정각원추도법 공식)
// ---------------------------------------------------------------------------

void latLonToGrid(double lat, double lon, int* nx, int* ny) {
  const double RE = 6371.00877, GRID = 5.0, SLAT1 = 30.0, SLAT2 = 60.0;
  const double OLON = 126.0, OLAT = 38.0, XO = 43, YO = 136;
  const double kPi = 3.14159265358979323846, DEGRAD = kPi / 180.0;

  double re = RE / GRID;
  double slat1 = SLAT1 * DEGRAD, slat2 = SLAT2 * DEGRAD;
  double olon = OLON * DEGRAD, olat = OLAT * DEGRAD;

  double sn = tan(kPi * 0.25 + slat2 * 0.5) / tan(kPi * 0.25 + slat1 * 0.5);
  sn = log(cos(slat1) / cos(slat2)) / log(sn);
  double sf = tan(kPi * 0.25 + slat1 * 0.5);
  sf = pow(sf, sn) * cos(slat1) / sn;
  double ro = tan(kPi * 0.25 + olat * 0.5);
  ro = re * sf / pow(ro, sn);

  double ra = tan(kPi * 0.25 + lat * DEGRAD * 0.5);
  ra = re * sf / pow(ra, sn);
  double theta = lon * DEGRAD - olon;
  if (theta > kPi) theta -= 2.0 * kPi;
  if (theta < -kPi) theta += 2.0 * kPi;
  theta *= sn;

  *nx = (int)floor(ra * sin(theta) + XO + 0.5);
  *ny = (int)floor(ro - ra * cos(theta) + YO + 0.5);
}

// ---------------------------------------------------------------------------
// 날짜 계산 (시간대와 상관없이 날짜만 더하고 빼려고 직접 계산)
// ---------------------------------------------------------------------------

static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const long yoe = y - era * 400;
  const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static void civilFromDays(long z, int* y, int* m, int* d) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const long doe = z - era * 146097;
  const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const long mp = (5 * doy + 2) / 153;
  *d = (int)(doy - (153 * mp + 2) / 5 + 1);
  *m = (int)(mp < 10 ? mp + 3 : mp - 9);
  *y = (int)(yoe + era * 400 + (*m <= 2));
}

void dateAfter(const struct tm& now, int days, char date[9], int* wday) {
  long z = daysFromCivil(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday) + days;
  int y, m, d;
  civilFromDays(z, &y, &m, &d);
  snprintf(date, 9, "%04d%02d%02d", y, m, d);
  if (wday) *wday = (int)(((z % 7) + 7 + 4) % 7);  // 1970-01-01 은 목요일(4)
}

// "HH00" (시는 0~23)
static void hourText(int hour, char time[5]) {
  time[0] = (char)('0' + (hour / 10) % 10);
  time[1] = (char)('0' + hour % 10);
  time[2] = '0';
  time[3] = '0';
  time[4] = '\0';
}

void forecastBase(const struct tm& now, bool today, char date[9], char time[5]) {
  const int minutes = now.tm_hour * 60 + now.tm_min;
  // 발표 후 10분쯤 지나야 받을 수 있습니다.
  int base = -1;
  if (today) {
    if (minutes >= 2 * 60 + 10) base = 2;
  } else {
    static const int bases[] = {2, 5, 8, 11, 14, 17, 20, 23};
    for (int b : bases) {
      if (minutes >= b * 60 + 10) base = b;
    }
  }
  if (base < 0) {  // 새벽이라 오늘 발표가 아직 없으면 어제 23시 발표
    dateAfter(now, -1, date, nullptr);
    hourText(23, time);
  } else {
    dateAfter(now, 0, date, nullptr);
    hourText(base, time);
  }
}

void nowcastBase(const struct tm& now, int hoursBack, char date[9], char time[5]) {
  int hour = now.tm_hour - hoursBack;
  if (now.tm_min < 15) hour--;
  int days = 0;
  while (hour < 0) {
    hour += 24;
    days--;
  }
  dateAfter(now, days, date, nullptr);
  hourText(hour, time);
}

// ---------------------------------------------------------------------------
// 요청 주소
// ---------------------------------------------------------------------------

static bool buildUrl(const char* op, int rows, const char* serviceKey, int nx, int ny,
                     const char* baseDate, const char* baseTime, char* out, size_t outSize) {
  if (!serviceKey || !*serviceKey) return false;
  char key[256];
  bus::encodeServiceKey(serviceKey, key, sizeof(key));
  int n = snprintf(out, outSize,
                   "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/%s"
                   "?serviceKey=%s&pageNo=1&numOfRows=%d&dataType=JSON"
                   "&base_date=%s&base_time=%s&nx=%d&ny=%d",
                   op, key, rows, baseDate, baseTime, nx, ny);
  return n > 0 && (size_t)n < outSize;
}

bool buildForecastUrl(const char* serviceKey, int nx, int ny, const char* baseDate,
                      const char* baseTime, char* out, size_t outSize) {
  return buildUrl("getVilageFcst", 1000, serviceKey, nx, ny, baseDate, baseTime, out, outSize);
}

bool buildNowcastUrl(const char* serviceKey, int nx, int ny, const char* baseDate,
                     const char* baseTime, char* out, size_t outSize) {
  return buildUrl("getUltraSrtNcst", 20, serviceKey, nx, ny, baseDate, baseTime, out, outSize);
}

// ---------------------------------------------------------------------------
// 응답 해석 도우미
// ---------------------------------------------------------------------------

// ArduinoJson 이 ByteReader 를 읽을 수 있게 이어 주는 통로 (한 글자 되돌리기 가능)
class JsonInput {
 public:
  explicit JsonInput(ByteReader& r) : r_(r) {}
  int read() {
    if (pending_ >= 0) {
      int c = pending_;
      pending_ = -1;
      return c;
    }
    return r_.read();
  }
  size_t readBytes(char* buf, size_t n) {
    size_t i = 0;
    for (; i < n; i++) {
      int c = read();
      if (c < 0) break;
      buf[i] = (char)c;
    }
    return i;
  }
  void unread(int c) { pending_ = c; }
  int nextNonSpace() {
    int c;
    do {
      c = read();
    } while (c == ' ' || c == '\t' || c == '\r' || c == '\n');
    return c;
  }

 private:
  ByteReader& r_;
  int pending_ = -1;
};

// "key" 를 찾아서 그 뒤의 ':' 까지 읽습니다.
static bool findKey(JsonInput& in, const char* key) {
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const size_t n = strlen(pattern);
  size_t matched = 0;
  for (;;) {
    int c = in.read();
    if (c < 0) return false;
    if (c == pattern[matched]) {
      if (++matched == n) break;
    } else {
      matched = (c == pattern[0]) ? 1 : 0;
    }
  }
  return in.nextNonSpace() == ':';
}

// ':' 다음의 "문자열" 값을 읽습니다.
static bool readString(JsonInput& in, char* out, size_t outSize) {
  if (in.nextNonSpace() != '"') return false;
  size_t i = 0;
  for (;;) {
    int c = in.read();
    if (c < 0) return false;
    if (c == '"') break;
    if (i + 1 < outSize) out[i++] = (char)c;
  }
  out[i] = '\0';
  return true;
}

// JSON 이 아닌 응답이면 앞부분을 모아서 이유를 적습니다.
static void describeNonJson(JsonInput& in, int first, char* error, size_t errorSize) {
  char buf[600];
  size_t n = 0;
  if (first >= 0) buf[n++] = (char)first;
  int c;
  while (n + 1 < sizeof(buf) && (c = in.read()) >= 0) buf[n++] = (char)c;
  buf[n] = '\0';
  if (n == 0) {
    bus::copyUtf8(error, errorSize, "빈 응답");
    return;
  }
  bus::describeErrorBody(buf, n, error, errorSize);
}

static double toNumber(JsonVariantConst v, bool* ok) {
  *ok = true;
  if (v.is<double>()) return v.as<double>();
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    char* end = nullptr;
    double d = strtod(s, &end);
    if (end != s) return d;
  }
  *ok = false;
  return 0;
}

static int roundToInt(double v) { return (int)floor(v + 0.5); }

static void resultError(char* error, size_t errorSize, const char* code, const char* msg) {
  if (strcmp(code, "03") == 0) {
    bus::copyUtf8(error, errorSize, "날씨 자료가 아직 없음 (발표 직후일 수 있음)");
    return;
  }
  char buf[128];
  snprintf(buf, sizeof(buf), "날씨 API 오류 %s: %s", code, msg);
  bus::copyUtf8(error, errorSize, buf);
}

// ---------------------------------------------------------------------------
// 단기예보 (조금씩 읽기)
// ---------------------------------------------------------------------------

bool parseForecast(ByteReader& reader, const char* targetDate, Day& out, char* error,
                   size_t errorSize) {
  out.ok = false;
  bus::copyUtf8(out.date, sizeof(out.date), targetDate);
  out.tmin10 = UNKNOWN10;
  out.tmax10 = UNKNOWN10;
  for (Hour& h : out.hours) h = {UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN};
  if (errorSize) error[0] = '\0';

  JsonInput in(reader);
  int first = in.nextNonSpace();
  if (first != '{') {
    describeNonJson(in, first, error, errorSize);
    return false;
  }

  // 1) 결과 코드 확인 ("00" 이 정상)
  char code[8] = "";
  if (!findKey(in, "resultCode") || !readString(in, code, sizeof(code))) {
    bus::copyUtf8(error, errorSize, "날씨 응답 형식이 예상과 다름");
    return false;
  }
  if (strcmp(code, "00") != 0) {
    char msg[64] = "";
    if (findKey(in, "resultMsg")) readString(in, msg, sizeof(msg));
    resultError(error, errorSize, code, msg);
    return false;
  }

  // 2) "item": [ {...}, {...} ] 을 하나씩 읽기
  if (!findKey(in, "item") || in.nextNonSpace() != '[') {
    bus::copyUtf8(error, errorSize, "날씨 자료가 비어 있음");
    return false;
  }

  JsonDocument filter;
  filter["category"] = true;
  filter["fcstDate"] = true;
  filter["fcstTime"] = true;
  filter["fcstValue"] = true;

  int found = 0;
  for (;;) {
    int c = in.nextNonSpace();
    if (c == ']') break;
    if (c != '{') {
      if (found == 0) {
        bus::copyUtf8(error, errorSize, "날씨 응답이 중간에 끊김");
        return false;
      }
      break;  // 이미 받은 만큼은 씀
    }
    in.unread(c);

    JsonDocument item;
    DeserializationError err =
        deserializeJson(item, in, DeserializationOption::Filter(filter));
    if (err) {
      if (found == 0) {
        bus::copyUtf8(error, errorSize, "날씨 응답이 중간에 끊김");
        return false;
      }
      break;
    }

    const char* date = item["fcstDate"] | "";
    int cmp = strcmp(date, targetDate);
    if (cmp > 0) break;  // 원하는 날짜를 지났으니 더 읽지 않음
    if (cmp == 0) {
      const char* cat = item["category"] | "";
      long hhmm = strtol(item["fcstTime"] | "", nullptr, 10);
      int hour = (int)(hhmm / 100);
      bool ok;
      double v = toNumber(item["fcstValue"], &ok);
      if (ok && hour >= 0 && hour < 24) {
        Hour& h = out.hours[hour];
        if (strcmp(cat, "TMP") == 0) h.temp = (int8_t)roundToInt(v);
        else if (strcmp(cat, "SKY") == 0) h.sky = (int8_t)v;
        else if (strcmp(cat, "PTY") == 0) h.pty = (int8_t)v;
        else if (strcmp(cat, "POP") == 0) h.pop = (int8_t)v;
        else if (strcmp(cat, "TMN") == 0) out.tmin10 = (int16_t)roundToInt(v * 10);
        else if (strcmp(cat, "TMX") == 0) out.tmax10 = (int16_t)roundToInt(v * 10);
        found++;
      }
    }

    c = in.nextNonSpace();
    if (c == ',') continue;
    break;  // ']' 또는 끝
  }

  if (found == 0) {
    bus::copyUtf8(error, errorSize, "그날 예보가 응답에 없음");
    return false;
  }

  // 최저/최고 기온이 없으면 (발표 시각에 따라 빠짐) 시간별 기온으로 계산
  int lo = 1000, hi = -1000;
  for (const Hour& h : out.hours) {
    if (h.temp == UNKNOWN) continue;
    if (h.temp < lo) lo = h.temp;
    if (h.temp > hi) hi = h.temp;
  }
  if (out.tmin10 == UNKNOWN10 && lo != 1000) out.tmin10 = (int16_t)(lo * 10);
  if (out.tmax10 == UNKNOWN10 && hi != -1000) out.tmax10 = (int16_t)(hi * 10);

  out.ok = true;
  return true;
}

// ---------------------------------------------------------------------------
// 초단기실황
// ---------------------------------------------------------------------------

bool parseNowcast(const char* body, size_t len, Now& out, char* error, size_t errorSize) {
  out.ok = false;
  out.temp10 = UNKNOWN10;
  out.pty = UNKNOWN;
  out.humidity = UNKNOWN;
  if (errorSize) error[0] = '\0';

  size_t i = 0;
  while (i < len && (body[i] == ' ' || body[i] == '\r' || body[i] == '\n' || body[i] == '\t')) i++;
  if (i >= len || body[i] != '{') {
    if (i >= len) {
      bus::copyUtf8(error, errorSize, "빈 응답");
    } else {
      bus::describeErrorBody(body + i, len - i, error, errorSize);
    }
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, body + i, len - i)) {
    bus::copyUtf8(error, errorSize, "날씨 응답 해석 실패");
    return false;
  }
  JsonObjectConst resp = doc["response"];
  const char* code = resp["header"]["resultCode"] | "";
  if (strcmp(code, "00") != 0) {
    resultError(error, errorSize, code[0] ? code : "?", resp["header"]["resultMsg"] | "");
    return false;
  }

  JsonVariantConst list = resp["body"]["items"]["item"];
  auto handle = [&](JsonObjectConst item) {
    const char* cat = item["category"] | "";
    bool ok;
    double v = toNumber(item["obsrValue"], &ok);
    if (!ok) return;
    if (strcmp(cat, "T1H") == 0) out.temp10 = (int16_t)roundToInt(v * 10);
    else if (strcmp(cat, "PTY") == 0) out.pty = (int8_t)v;
    else if (strcmp(cat, "REH") == 0) out.humidity = (int8_t)v;
  };
  if (list.is<JsonArrayConst>()) {
    for (JsonObjectConst item : list.as<JsonArrayConst>()) handle(item);
  } else if (list.is<JsonObjectConst>()) {
    handle(list.as<JsonObjectConst>());
  }

  if (out.temp10 == UNKNOWN10) {
    bus::copyUtf8(error, errorSize, "현재 기온이 응답에 없음");
    return false;
  }
  out.ok = true;
  return true;
}

// ---------------------------------------------------------------------------
// 날씨 종류 정리
// ---------------------------------------------------------------------------

static Condition skyCondition(int8_t sky) {
  switch (sky) {
    case 1: return COND_CLEAR;
    case 3: return COND_PARTLY;
    case 4: return COND_CLOUDY;
    default: return COND_UNKNOWN;
  }
}

Condition hourCondition(const Hour& h) {
  switch (h.pty) {
    case 1: return COND_RAIN;
    case 2: return COND_RAIN_SNOW;
    case 3: return COND_SNOW;
    case 4: return COND_SHOWER;
    default: return skyCondition(h.sky);
  }
}

Condition nowCondition(const Now& now, int8_t sky) {
  switch (now.pty) {
    case 1:
    case 5: return COND_RAIN;
    case 2:
    case 6: return COND_RAIN_SNOW;
    case 3:
    case 7: return COND_SNOW;
    default: return skyCondition(sky);
  }
}

Condition summarize(const Day& day, int fromHour, int toHour, int* maxPop) {
  int counts[8] = {0};
  int pop = -1;
  bool wet = false;
  for (int h = fromHour; h < toHour && h < 24; h++) {
    if (h < 0) continue;
    const Hour& hr = day.hours[h];
    if (hr.pop != UNKNOWN && hr.pop > pop) pop = hr.pop;
    Condition c = hourCondition(hr);
    if (c == COND_UNKNOWN) continue;
    counts[c]++;
    if (c >= COND_RAIN) wet = true;
  }
  if (maxPop) *maxPop = pop;

  // 비나 눈이 한 시간이라도 있으면 그걸 대표로, 아니면 가장 많은 하늘 상태 (같으면 더 흐린 쪽)
  Condition best = COND_UNKNOWN;
  int bestCount = 0;
  int from = wet ? COND_RAIN : COND_CLEAR;
  int to = wet ? COND_SHOWER : COND_CLOUDY;
  for (int c = from; c <= to; c++) {
    if (counts[c] > 0 && counts[c] >= bestCount) {
      best = (Condition)c;
      bestCount = counts[c];
    }
  }
  return best;
}

}  // namespace weather
