// 인터넷으로 기상청 날씨를 받아 옵니다 (Pico W / ESP32).
#pragma once

#include "weather_api.h"

// 날씨 화면 하나에 필요한 정보
struct WeatherData {
  bool ok;
  bool tomorrow;     // true = 내일 날씨, false = 오늘 날씨
  char error[96];
  int month, mday;   // 날씨 날짜
  int wday;          // 요일 (0 = 일요일)
  weather::Day day;  // 시간별 예보, 최저/최고
  weather::Now now;  // 지금 기온 (오늘 날씨일 때만. 실패해도 예보는 보여줌)
};

// 성공하면 true. 실패하면 false 이고 out.error 에 이유가 들어 있습니다.
bool fetchWeather(bool tomorrow, WeatherData& out);
