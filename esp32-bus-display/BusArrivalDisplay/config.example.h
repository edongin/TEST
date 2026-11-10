// =============================================================================
//  설정 파일
//  이 파일을 같은 폴더에 config.h 라는 이름으로 복사한 뒤 값을 채우세요.
//  (config.h 는 비밀번호와 인증키가 들어가므로 git 에 올라가지 않습니다)
// =============================================================================
#pragma once

// ----- Wi-Fi (2.4GHz 만 됩니다) -----
#define WIFI_SSID     "와이파이이름"
#define WIFI_PASSWORD "와이파이비밀번호"

// ----- 어느 지역 API 를 쓸지 -----
//   1 = 경기도 (경기도_버스도착정보 조회 서비스)
//   2 = 서울시 (서울특별시_정류소정보조회 서비스, getStationByUid)
//   3 = 그 외 지역 (국토교통부_(TAGO)_버스도착정보)
#define BUS_PROVIDER 1

// ----- 공공데이터포털(data.go.kr) 인증키 -----
// "일반 인증키(Encoding)" 또는 "(Decoding)" 어느 쪽을 넣어도 됩니다.
#define SERVICE_KEY "여기에_인증키를_붙여넣기"

// ----- 정류장 -----
//   경기도: 정류소 ID (9자리 숫자, 예: "208000123")
//   서울:   정류소 고유번호 (정류장 표지판의 5자리 숫자, 예: "01234")
//   TAGO:   정류소 ID (예: "DJB8001793")
#define STATION_ID "208000123"

// TAGO 를 쓸 때만: 도시 코드 (예: 대전 "25"). 다른 지역은 비워 두세요.
#define TAGO_CITY_CODE ""

// 화면 맨 위에 보일 정류장 이름. 경기도 API 는 정류장 이름을 주지 않아서 직접 적어 주세요.
// 비워 두면 API 가 알려주는 이름을 씁니다 (서울, TAGO).
#define STATION_LABEL "우리집 앞 정류장"

// 보고 싶은 노선만 쉼표로 적기 (예: "11-3, 5, 9"). 비워 두면 전부 보여줍니다.
#define ROUTE_FILTER ""

// true 면 먼저 오는 버스가 위로, false 면 API 가 준 순서 그대로
#define SORT_BY_ARRIVAL true

// ----- 갱신 주기 -----
// 개발계정 인증키는 하루 호출 수가 제한됩니다(보통 1,000회 정도).
// 90초 간격으로 하루 종일 켜 두면 960회. 갱신 사이에는 화면이 알아서 시간을 줄여 갑니다.
#define UPDATE_INTERVAL_SEC 90

// 이 시간대에만 화면을 켜고 API 를 부릅니다 (호출 수 절약). 0 과 24 면 하루 종일.
#define ACTIVE_START_HOUR 5   // 05:00 부터
#define ACTIVE_END_HOUR   24  // 24:00 까지

// 노선이 한 화면에 다 안 들어가면 몇 초마다 다음 페이지로 넘길지
#define PAGE_INTERVAL_SEC 6

// ----- 보드 종류 -----
//   0 = ESP32 개발보드 + ILI9341 SPI 화면 모듈을 점퍼선으로 연결 (README 의 연결표대로)
//   1 = ESP32-2432S028R (일명 CYD, 화면 일체형 보드)
#define BOARD_CYD 0

#if BOARD_CYD
// CYD 는 보드 안에서 이미 연결되어 있으므로 바꿀 필요 없음
#define TFT_SCLK 14
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_USE_HSPI 1
#else
// ESP32 개발보드(ESP32-WROOM-32) + ILI9341 3.2인치/2.8인치 SPI 모듈
// 화면 핀 -> ESP32 핀 (VCC 는 3V3, GND 는 GND 에 연결)
#define TFT_SCLK 18   // SCK
#define TFT_MISO 19   // SDO(MISO)
#define TFT_MOSI 23   // SDI(MOSI)
#define TFT_CS   5    // CS
#define TFT_DC   26   // DC (D/C, RS 라고 적힌 모듈도 있음)
#define TFT_RST  4    // RESET
#define TFT_BL   22   // LED (백라이트). 3V3 에 직접 꽂았다면 -1
#define TFT_USE_HSPI 0
#endif

// 화면 통신 속도. 점퍼선이 길어서 화면에 줄이 가거나 깨지면 20000000 으로 낮추세요.
#define TFT_SPI_HZ 27000000

#define TFT_ROTATION 1     // 1 또는 3 = 가로 화면. 화면이 뒤집혀 보이면 1 <-> 3
#define TFT_INVERT_COLORS false  // 색이 반전돼 보이면 true

// 이 버튼을 누르면 바로 새로고침 (개발보드와 CYD 모두 BOOT 버튼 = 0번 핀). 안 쓰면 -1
#define REFRESH_BUTTON_PIN 0

// 나중에 터치를 쓸 때의 배선 (지금 버스 알림판 코드에서는 쓰지 않음)
// 터치 핀 -> ESP32 핀: T_CLK 25, T_DIN 32, T_DO 39(VN), T_CS 33, T_IRQ 36(VP)
