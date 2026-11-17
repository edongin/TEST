// 기상청 단기예보 / 초단기실황 API: 요청 주소 만들기와 응답 해석
// Arduino 기능을 쓰지 않아서 PC에서도 테스트할 수 있습니다.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

namespace weather {

constexpr int8_t UNKNOWN = -128;      // 값이 없을 때
constexpr int16_t UNKNOWN10 = -32768;  // 10배 값이 없을 때

// 하늘 상태 + 강수 형태를 합친 날씨 종류
enum Condition : int8_t {
  COND_UNKNOWN = 0,
  COND_CLEAR,      // 맑음
  COND_PARTLY,     // 구름많음
  COND_CLOUDY,     // 흐림
  COND_RAIN,       // 비
  COND_RAIN_SNOW,  // 비/눈
  COND_SNOW,       // 눈
  COND_SHOWER,     // 소나기
};

const char* conditionName(Condition c);

// 한 시간의 예보
struct Hour {
  int8_t temp;  // 기온 (도). UNKNOWN = 없음
  int8_t sky;   // 1 맑음, 3 구름많음, 4 흐림
  int8_t pty;   // 0 없음, 1 비, 2 비/눈, 3 눈, 4 소나기
  int8_t pop;   // 강수확률 (%)
};

// 하루치 예보 (단기예보 한 번 호출로 채움)
struct Day {
  bool ok;
  char date[9];      // "20261117"
  int16_t tmin10;    // 최저기온 x10. UNKNOWN10 = 없음
  int16_t tmax10;    // 최고기온 x10
  Hour hours[24];
};

// 지금 날씨 (초단기실황)
struct Now {
  bool ok;
  int16_t temp10;  // 기온 x10
  int8_t pty;      // 0 없음, 1 비, 2 비/눈, 3 눈, 5 빗방울, 6 빗방울눈날림, 7 눈날림
  int8_t humidity; // 습도 (%)
};

// 위도·경도를 기상청 격자 좌표(nx, ny)로 바꿉니다.
void latLonToGrid(double lat, double lon, int* nx, int* ny);

// 요청할 발표 시각 고르기 (now 는 한국 시간)
//   today=true : 오늘 하루 전체(최저/최고 포함)가 들어 있는 오늘 02시 발표
//   today=false: 지금 받을 수 있는 가장 최근 발표 (내일 날씨용)
void forecastBase(const struct tm& now, bool today, char date[9], char time[5]);
// 초단기실황 발표 시각 (매시 정각 발표, 15분 이후부터 받음). hoursBack 만큼 더 이전 것.
void nowcastBase(const struct tm& now, int hoursBack, char date[9], char time[5]);

// now 로부터 days 일 뒤 날짜 ("YYYYMMDD")와 요일(0=일)
void dateAfter(const struct tm& now, int days, char date[9], int* wday);

bool buildForecastUrl(const char* serviceKey, int nx, int ny, const char* baseDate,
                      const char* baseTime, char* out, size_t outSize);
bool buildNowcastUrl(const char* serviceKey, int nx, int ny, const char* baseDate,
                     const char* baseTime, char* out, size_t outSize);

// 한 글자씩 읽는 통로. 인터넷 응답(Arduino)과 문자열(PC 테스트) 둘 다 이걸로 읽습니다.
class ByteReader {
 public:
  virtual ~ByteReader() {}
  virtual int read() = 0;  // 다 읽었거나 시간이 지나면 -1
};

class StringReader : public ByteReader {
 public:
  StringReader(const char* s, size_t len) : s_(s), len_(len) {}
  int read() override { return pos_ < len_ ? (unsigned char)s_[pos_++] : -1; }

 private:
  const char* s_;
  size_t len_;
  size_t pos_ = 0;
};

// 단기예보 응답을 조금씩 읽으며 targetDate 날짜만 골라 담습니다.
// 응답이 커서(수십 KB) 한꺼번에 메모리에 올리지 않습니다. 원하는 날짜가 지나면 바로 멈춥니다.
bool parseForecast(ByteReader& in, const char* targetDate, Day& out, char* error,
                   size_t errorSize);

// 초단기실황 응답 (작아서 한 번에 해석)
bool parseNowcast(const char* body, size_t len, Now& out, char* error, size_t errorSize);

// 하루 중 시간대(fromHour 이상 toHour 미만)의 대표 날씨와 최대 강수확률
Condition summarize(const Day& day, int fromHour, int toHour, int* maxPop);
// 한 시간의 날씨 종류
Condition hourCondition(const Hour& h);
// 초단기실황 강수 형태 + 그 시간 하늘 상태로 지금 날씨 종류
Condition nowCondition(const Now& now, int8_t sky);

}  // namespace weather
