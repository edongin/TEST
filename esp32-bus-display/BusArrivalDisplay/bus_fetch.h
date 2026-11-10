// 인터넷으로 버스 도착정보를 받아 옵니다 (ESP32 전용).
#pragma once

#include "bus_api.h"

// config.h 설정대로 API 를 호출합니다.
// 성공하면 true. 실패하면 false 이고 out.error 에 이유가 들어 있습니다.
bool fetchArrivals(bus::Result& out);
