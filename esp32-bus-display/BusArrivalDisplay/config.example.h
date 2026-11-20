// =============================================================================
//  설정 파일
//  이 파일을 같은 폴더에 config.h 라는 이름으로 복사한 뒤 값을 채우세요.
//  (config.h 는 비밀번호와 인증키가 들어가므로 git 에 올라가지 않습니다)
// =============================================================================
#pragma once

// ----- Wi-Fi (2.4GHz 만 됩니다) -----
#define WIFI_SSID     "와이파이이름"
#define WIFI_PASSWORD "와이파이비밀번호"

// ----- 공공데이터포털(data.go.kr) 인증키 -----
// "일반 인증키(Encoding)" 또는 "(Decoding)" 어느 쪽을 넣어도 됩니다.
// 버스와 날씨 모두 이 키 하나로 씁니다 (각 API 를 활용신청만 해 두면 됨).
#define SERVICE_KEY "여기에_인증키를_붙여넣기"

// =============================================================================
//  버스
// =============================================================================

// 어느 지역 API 를 쓸지
//   1 = 경기도 (경기도_버스도착정보 조회 서비스)
//   2 = 서울시 (서울특별시_정류소정보조회 서비스)
//   3 = 그 외 지역 (국토교통부_(TAGO)_버스도착정보)
#define BUS_PROVIDER 1

// TAGO 를 쓸 때만: 도시 코드 (예: 대전 "25"). 다른 지역은 비워 두세요.
#define TAGO_CITY_CODE ""

// 정류장 (최대 3곳 권장). 화면을 터치할 때마다 1 → 2 → 3 → 날씨 순서로 바뀝니다.
//   이름   : 화면 맨 위에 보일 이름
//   정류장 ID
//     경기도: 정류소 ID (9자리 숫자, 예: "208000123")
//     서울:   정류소 고유번호 (정류장 표지판의 5자리 숫자, 예: "01234")
//     TAGO:   정류소 ID (예: "DJB8001793")
//   노선   : 보고 싶은 노선만 쉼표로 (예: "11-3, 5"). 비워 두면 전부.
static const StationConfig STATIONS[] = {
    // 이름              정류장 ID     노선
    {"집 앞 (서울 방향)", "208000123", ""},
    {"집 앞 (안양 방향)", "208000124", ""},
    {"회사 앞",          "208000456", ""},
};

// 정류장 화면에서 몇 초마다 새 정보를 받을지
#define BUS_UPDATE_SEC 15

// 정류장 화면을 띄운 뒤 몇 분 동안 보여줄지. 지나면 기본 화면(날씨 또는 꺼짐)으로 돌아갑니다.
#define STATION_SHOW_MIN 30

// true 면 먼저 오는 버스가 위로, false 면 API 가 준 순서 그대로
#define SORT_BY_ARRIVAL true

// 노선이 한 화면에 다 안 들어가면 몇 초마다 다음 페이지로 넘길지
#define PAGE_INTERVAL_SEC 6

// =============================================================================
//  날씨 (기상청 단기예보)
// =============================================================================

// 날씨를 볼 위치의 위도, 경도 (지도 앱에서 위치를 길게 누르면 나옵니다)
#define WEATHER_LAT 37.5665   // 예: 서울시청
#define WEATHER_LON 126.9780

// 날씨 화면 제목에 붙일 동네 이름
#define WEATHER_LABEL "우리 동네"

// 기본 화면 시간표 (시작 시각 이상, 끝 시각 미만)
//   오늘 날씨: 오전에 켜 두기
#define TODAY_WEATHER_START 6    // 06:00 부터
#define TODAY_WEATHER_END   12   // 12:00 전까지
//   내일 날씨: 저녁에 켜 두기
#define TOMORROW_WEATHER_START 20  // 20:00 부터
#define TOMORROW_WEATHER_END   23  // 23:00 전까지
//   그 밖의 시간에는 화면이 꺼져 있고, 터치하면 정류장 1 이 켜집니다.

// 날씨를 몇 분마다 새로 받을지 (기상청 발표는 3시간마다)
#define WEATHER_UPDATE_MIN 30

// =============================================================================
//  보드와 화면 연결
// =============================================================================
// Arduino IDE 에서 고른 보드에 따라 아래 중 하나가 자동으로 쓰입니다.
//   - "Raspberry Pi Pico W" 를 고르면  -> [A] Pico WH(브레드보드) + ILI9341 모듈
//   - "ESP32 Dev Module" 을 고르면     -> [B] ESP32 개발보드 + ILI9341 모듈, 또는 [C] CYD

#if defined(ARDUINO_ARCH_RP2040)
// [A] Raspberry Pi Pico WH (또는 Pico W, Pico 2 W) + ILI9341 3.2인치 SPI 모듈
//     README 의 "Pico WH 배선표" 대로 연결하세요.
// 터치 연결 방식
//   0 = 브레드보드: 터치가 화면과 SPI 선을 같이 씀 (GP2, GP3 에 선이 2개씩)
//   1 = F/F 선으로 1:1 연결: 터치를 다른 핀(SPI1)에 따로 연결
//       T_CLK -> GP10, T_DIN -> GP11, T_DO -> GP12 (나머지는 같음)
#define TOUCH_SEPARATE_PINS 0

#define TFT_SCLK 2    // 화면 SCK
#define TFT_MOSI 3    // 화면 SDI
#define TFT_CS   1    // 화면 CS
#define TFT_DC   21   // 화면 D/C
#define TFT_RST  20   // 화면 RESET
#define TFT_BL   22   // 화면 LED (백라이트). 3.3V 에 바로 꽂았다면 -1 (그러면 화면을 완전히 끌 수 없음)

#define TOUCH_CS  15  // 터치 T_CS
#define TOUCH_IRQ 28  // 터치 T_IRQ

#if TOUCH_SEPARATE_PINS
#define TFT_MISO   -1  // 화면의 SDO 는 연결하지 않음
#define TOUCH_SCLK 10  // 터치 T_CLK
#define TOUCH_MOSI 11  // 터치 T_DIN
#define TOUCH_MISO 12  // 터치 T_DO
#define TOUCH_SHARED_SPI 0
#else
#define TFT_MISO 0     // 터치 T_DO (화면의 SDO 는 연결하지 않음). T_CLK 는 GP2, T_DIN 은 GP3 에 같이
#define TOUCH_SHARED_SPI 1
#endif

// 터치 대신 쓸 수 있는 버튼: Pico 위의 흰색 BOOTSEL 버튼. 버튼을 따로 달았다면 그 핀 번호.
#define NEXT_BUTTON_PIN -1
#define USE_BOOTSEL_BUTTON 1

#else
// ESP32 에서는 0 = [B] 개발보드 + ILI9341 모듈, 1 = [C] CYD
#define BOARD_CYD 0

#if BOARD_CYD
// [C] ESP32-2432S028R (CYD). 보드 안에서 이미 연결되어 있으므로 바꿀 필요 없음
#define TFT_SCLK 14
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_USE_HSPI 1
#else
// [B] ESP32 개발보드(ESP32-WROOM-32) + ILI9341 SPI 모듈
#define TFT_SCLK 18   // SCK
#define TFT_MISO 19   // SDO(MISO)
#define TFT_MOSI 23   // SDI(MOSI)
#define TFT_CS   5    // CS
#define TFT_DC   26   // DC
#define TFT_RST  4    // RESET
#define TFT_BL   22   // LED (백라이트). 3V3 에 직접 꽂았다면 -1
#define TFT_USE_HSPI 0
#endif

// 터치 (개발보드와 CYD 모두 같은 핀)
#define TOUCH_SCLK 25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_SHARED_SPI 0

// 터치 대신 쓸 수 있는 버튼 (BOOT 버튼 = 0번 핀). 안 쓰면 -1
#define NEXT_BUTTON_PIN 0
#define USE_BOOTSEL_BUTTON 0
#endif

// 터치 감지 방법
//   1 = T_IRQ 신호 또는 누르는 힘으로 감지 (기본, 화면·터치 테스트와 같은 방식)
//   2 = 누르는 힘만으로 감지 (손대지 않았는데 화면이 저절로 넘어갈 때 바꿔 보세요)
#define TOUCH_MODE 1

// 화면 통신 속도. 점퍼선이 길어서 화면에 줄이 가거나 깨지면 20000000 으로 낮추세요.
#define TFT_SPI_HZ 27000000

#define TFT_ROTATION 1     // 1 또는 3 = 가로 화면. 화면이 뒤집혀 보이면 1 <-> 3
#define TFT_INVERT_COLORS true   // IPS 화면은 true. 색이 반대로 보이면 false 로 바꾸세요
