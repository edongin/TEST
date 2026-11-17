// 인터넷으로 버스 도착정보를 받아 옵니다 (Pico W / ESP32).
#pragma once

#include "app_config.h"
#include "bus_api.h"

// 정류장 하나의 도착 정보를 받아 옵니다.
// 성공하면 true. 실패하면 false 이고 out.error 에 이유가 들어 있습니다.
bool fetchArrivals(const StationConfig& station, bus::Result& out);
