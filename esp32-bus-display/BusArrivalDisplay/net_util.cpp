#include "net_util.h"

#include <Arduino.h>
#include <WiFi.h>
#if !defined(ARDUINO_ARCH_RP2040)
#include <WiFiClientSecure.h>
#endif

bool httpBegin(HTTPClient& http, const char* url) {
  const bool https = strncmp(url, "https://", 8) == 0;
  http.setTimeout(10000);
#if defined(ARDUINO_ARCH_RP2040)
  // Pico W: HTTPClient 가 https 연결을 직접 만듭니다.
  // (setInsecure 는 https 연결을 만들어 버리므로 https 주소일 때만 부릅니다)
  if (https) http.setInsecure();
  return http.begin(String(url));
#else
  // ESP32: 연결 통로는 http 보다 오래 살아 있어야 해서 static 으로 둡니다.
  static WiFiClientSecure secureClient;
  static WiFiClient plainClient;
  http.setConnectTimeout(8000);
  if (https) {
    secureClient.setInsecure();
    return http.begin(secureClient, url);
  }
  return http.begin(plainClient, url);
#endif
}
