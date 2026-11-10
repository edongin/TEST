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

// ----- 보드와 화면 연결 -----
// Arduino IDE 에서 고른 보드에 따라 아래 셋 중 하나가 자동으로 쓰입니다.
//   - "Raspberry Pi Pico W" 를 고르면  -> [A] UHF Reader for Pico W 보드 + ILI9341 모듈
//   - "ESP32 Dev Module" 을 고르면     -> [B] ESP32 개발보드 + ILI9341 모듈, 또는 [C] CYD

#if defined(ARDUINO_ARCH_RP2040)
// [A] SB Components "UHF Reader for Pico W" + ILI9341 3.2인치 SPI 모듈
//     README 의 "Pico W 배선표" 대로 연결하세요.
#define UHF_READER_BOARD 1  // 일반 Pico W 에 직접 연결했다면 0

#define TFT_SCLK 2    // 화면 SCK   (터치 T_CLK 도 같은 핀에)
#define TFT_MOSI 3    // 화면 SDI   (터치 T_DIN 도 같은 핀에)
#define TFT_MISO 0    // 터치 T_DO  (화면의 SDO 는 연결하지 않음)
#define TFT_CS   1    // 화면 CS
#define TFT_DC   15   // 화면 DC
#define TFT_RST  -1   // 화면 RESET 은 3V3 에 연결 (핀이 모자라서 명령으로 초기화)
#define TFT_BL   28   // 화면 LED (백라이트). 3V3 에 직접 꽂았다면 -1

// 이 버튼을 누르면 바로 새로고침 (보드의 BT2 버튼). 안 쓰면 -1
#define REFRESH_BUTTON_PIN 10

// UHF 리더 보드에 이미 붙어 있는 부품 (바꿀 필요 없음)
#define UHF_EN_PIN     26  // RFID 모듈 켜기/끄기
#define UHF_BUZZER_PIN 22  // 부저
#define UHF_LCD_BL_PIN 12  // 보드의 1.14인치 작은 화면 백라이트
#define UHF_SD_CS_PIN  17  // SD 카드

// 나중에 터치를 쓸 때: T_CS -> GP21, T_IRQ -> GP20 (T_CLK, T_DIN, T_DO 는 위와 같은 핀)

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
// 나중에 터치: T_CLK 25, T_DIN 32, T_DO 39(VN), T_CS 33, T_IRQ 36(VP)
#endif

// 이 버튼을 누르면 바로 새로고침 (BOOT 버튼 = 0번 핀). 안 쓰면 -1
#define REFRESH_BUTTON_PIN 0
#endif

// 화면 통신 속도. 점퍼선이 길어서 화면에 줄이 가거나 깨지면 20000000 으로 낮추세요.
#define TFT_SPI_HZ 27000000

#define TFT_ROTATION 1     // 1 또는 3 = 가로 화면. 화면이 뒤집혀 보이면 1 <-> 3
#define TFT_INVERT_COLORS false  // 색이 반전돼 보이면 true
