// 인터넷 요청을 시작하는 공통 코드 (Pico W / ESP32)
#pragma once

#include <HTTPClient.h>

// http(s) 주소로 요청을 준비합니다. https 는 서버 인증서 검사를 생략합니다.
// (공개된 버스·날씨 정보라서, 인증서가 바뀌어도 멈추지 않는 쪽을 택했습니다)
bool httpBegin(HTTPClient& http, const char* url);
