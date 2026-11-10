// config.h 를 불러옵니다. 없으면 무엇을 해야 하는지 알려 주는 오류를 냅니다.
#pragma once

#if __has_include("config.h")
#include "config.h"
#else
#error "config.example.h 를 같은 폴더에 config.h 로 복사하고 Wi-Fi, 인증키, 정류장 정보를 채워 주세요."
#endif
