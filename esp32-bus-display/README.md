# ESP32 버스 도착 알림판

공공데이터포털의 **버스 도착정보 API**를 불러와서, ESP32에 연결한 ILI9341 화면(2.8인치/3.2인치)에 우리 집 앞 정류장의 버스 도착 시간을 보여줍니다.

```
┌──────────────────────────────────────────┐
│ 우리집 앞 정류장                    14:05 │  ← 정류장 이름, 현재 시각
├──────────────────────────────────────────┤
│ 3330    곧 도착 (1정거장 전) 23석          │  ← 3분 이내는 주황색
│         다음 18분 / 강남역 방면            │
│ 11-3    6분 (4정거장 전)                  │
│         다음 21분 / 범계역 방면            │
│ 5       도착 정보 없음                    │  ← 정보 없으면 회색
│                                          │
├──────────────────────────────────────────┤
│ 14:04:31 갱신                        1/2 │  ← 노선이 많으면 페이지가 넘어감
└──────────────────────────────────────────┘
```

**기능**
- 경기도, 서울, 그 외 지역(TAGO) API 지원
- 첫 번째와 두 번째 버스의 남은 시간, 남은 정류장 수, 빈자리 수(경기도 광역버스) 표시
- API를 부르지 않는 동안에도 남은 시간이 화면에서 계속 줄어듦 (호출 수 절약)
- 먼저 오는 버스부터 정렬, 원하는 노선만 골라 보기
- 밤에는 화면 끄기, BOOT 버튼으로 바로 새로고침
- Wi-Fi가 끊기거나 API가 실패하면 마지막 정보를 그대로 두고 아래에 경고만 표시

---

## 1. 준비물

### 기본: ESP32 개발보드 + ILI9341 화면 모듈 (점퍼선 연결, 납땜 없음)
| 부품 | 고를 때 기준 | 대략 가격 |
|---|---|---|
| **ILI9341 TFT LCD 모듈** (3.2인치 또는 2.8인치) | 설명에 **SPI**라고 적혀 있고, 한 줄에 핀이 14개 정도인 것. "16bit", "병렬", "메가용"은 안 됨 | 1.5만~2.5만 원 |
| **ESP32 개발보드** | **ESP32-WROOM-32** 칩, USB 단자 달린 것 (ESP32-DevKitC, NodeMCU-32S 등) | 1만~1.5만 원 |
| **점퍼선** | **암-암(Female-Female)**, 20cm 정도, 15가닥 이상 (터치까지 쓰면 14가닥 필요) | 2천 원 안팎 |
| USB 케이블 | 개발보드 단자에 맞고 **데이터 전송이 되는** 것 (충전 전용은 안 됨) | |
| 휴대폰 충전기 | 5V 1A 이상 | |

### 화면 연결 (9가닥)
| 화면 모듈 핀 | ESP32 개발보드 핀 | 하는 일 |
|---|---|---|
| VCC | **3V3** | 전원 |
| GND | GND | 접지 |
| CS | GPIO 5 | 화면 선택 |
| RESET | GPIO 4 | 화면 초기화 |
| DC (D/C, RS) | GPIO 26 | 명령/데이터 구분 |
| SDI (MOSI) | GPIO 23 | ESP32 → 화면 데이터 |
| SCK | GPIO 18 | 통신 박자 |
| LED | GPIO 22 | 백라이트 (밤에 끄기용) |
| SDO (MISO) | GPIO 19 | 화면 → ESP32 데이터 |

### 터치 연결 (5가닥, 나중에 터치 게임 등을 할 때)
지금 버스 알림판은 터치를 쓰지 않지만, 처음부터 함께 꽂아 두면 나중에 다시 배선할 필요가 없습니다.

| 화면 모듈 핀 | ESP32 개발보드 핀 |
|---|---|
| T_CLK | GPIO 25 |
| T_CS | GPIO 33 |
| T_DIN | GPIO 32 |
| T_DO | GPIO 39 (보드에 **VN** 또는 **SN**으로 적혀 있음) |
| T_IRQ | GPIO 36 (보드에 **VP** 또는 **SP**로 적혀 있음) |

> 개발보드마다 핀 순서가 다르므로, 보드에 인쇄된 핀 이름(예: `D23`, `G23`, `IO23`)을 보고 꽂으세요.
> 화면이 켜지지 않으면 LED 선을 GPIO 22 대신 **3V3**에 꽂고 `config.h`에서 `TFT_BL -1`로 바꿔 보세요.
> 배선은 **USB를 뺀 상태에서** 하세요.

### 다른 방법: CYD 보드 (화면 일체형)
**ESP32-2432S028R** (일명 CYD)은 ESP32와 2.8인치 화면이 붙어 있어 배선이 필요 없습니다. 이 보드를 쓴다면 `config.h`에서 `BOARD_CYD 1`로 바꾸기만 하면 됩니다.

---

## 2. API 인증키 받기 (공공데이터포털)

1. [data.go.kr](https://www.data.go.kr) 회원가입 후 로그인
2. 지역에 맞는 API를 검색해서 **활용신청** (개발계정은 보통 바로 승인됩니다)

   | 지역 | 검색할 이름 | config.h |
   |---|---|---|
   | 경기도 | `경기도_버스도착정보 조회` | `BUS_PROVIDER 1` |
   | 서울 | `서울특별시_정류소정보조회 서비스` | `BUS_PROVIDER 2` |
   | 그 외 지역 | `국토교통부_(TAGO)_버스도착정보` | `BUS_PROVIDER 3` |

3. **마이페이지 → 개발계정**에서 **일반 인증키**를 복사합니다. Encoding 키와 Decoding 키 중 어느 것을 넣어도 됩니다.

> 승인 직후에는 키가 서버에 반영되기까지 **1~2시간** 걸릴 수 있습니다. 그동안에는 화면에 "인증키 미등록"이 나옵니다.

---

## 3. 정류장 ID 찾기

### 서울
정류장 표지판에 적힌 **5자리 숫자**(정류소 고유번호)가 `STATION_ID`입니다. 예: `"01234"`

### 경기도
경기도 API는 9자리 **정류소 ID**(예: `208000123`)를 씁니다. 표지판의 5자리 번호와는 다릅니다.

1. data.go.kr에서 `경기도_정류소 조회`도 활용신청합니다.
2. 브라우저 주소창에 아래 주소를 넣습니다. `인증키`와 `정류장이름`은 바꿔서 넣으세요.
   ```
   https://apis.data.go.kr/6410000/busstationservice/v2/getBusStationListv2?serviceKey=인증키&keyword=정류장이름&format=json
   ```
3. 결과에서 원하는 정류장의 `stationId` 값을 씁니다. 같은 이름의 정류장이 길 양쪽에 있으면 `mobileNo`(표지판 번호)로 구분하세요.

경기도 API는 정류장 이름을 알려주지 않습니다. 그래서 `STATION_LABEL`에 화면에 보일 이름을 직접 적어 주세요.

### 그 외 지역 (TAGO)
1. data.go.kr에서 `국토교통부_(TAGO)_버스정류소정보`도 활용신청합니다.
2. 도시 코드 목록:
   ```
   https://apis.data.go.kr/1613000/BusSttnInfoInqireService/getCtyCodeList?serviceKey=인증키&_type=json
   ```
3. 정류장 찾기 (cityCode는 위에서 찾은 값):
   ```
   https://apis.data.go.kr/1613000/BusSttnInfoInqireService/getSttnNoList?serviceKey=인증키&cityCode=25&nodeNm=정류장이름&_type=json
   ```
4. 결과의 `nodeid`를 `STATION_ID`에, 도시 코드를 `TAGO_CITY_CODE`에 넣습니다.

---

## 4. 설정하기

`BusArrivalDisplay/config.example.h`를 같은 폴더에 **`config.h`** 라는 이름으로 복사한 뒤 값을 채웁니다.

```cpp
#define WIFI_SSID     "우리집와이파이"
#define WIFI_PASSWORD "비밀번호"
#define BUS_PROVIDER  1                  // 1 경기, 2 서울, 3 TAGO
#define SERVICE_KEY   "발급받은_인증키"
#define STATION_ID    "208000123"
#define STATION_LABEL "우리집 앞 정류장"
#define ROUTE_FILTER  "11-3, 3330"       // 비우면 전체 노선
```

`config.h`에는 비밀번호와 인증키가 들어가므로 git에 올라가지 않게 해 두었습니다.

---

## 5. 업로드하기

### 방법 A. Arduino IDE (처음이라면 추천)
1. [Arduino IDE](https://www.arduino.cc/en/software) 설치
2. **파일 → 기본 설정 → 추가 보드 관리자 URL**에 아래 주소를 넣습니다.
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. **보드 매니저**에서 `esp32` (Espressif Systems) 설치
4. **라이브러리 매니저**에서 아래 3개를 설치합니다. 의존 라이브러리도 같이 설치하라고 하면 "모두 설치"를 누르세요.
   - `ArduinoJson` (Benoit Blanchon, 7.x)
   - `Adafruit ILI9341`
   - `U8g2_for_Adafruit_GFX`
5. `BusArrivalDisplay/BusArrivalDisplay.ino` 열기
6. **도구** 메뉴 설정
   - 보드: `ESP32 Dev Module`
   - Partition Scheme: `Huge APP (3MB No OTA/1MB SPIFFS)`
   - 포트: 보드가 연결된 포트
7. 업로드(→) 버튼. "Connecting..."에서 멈추면 보드의 **BOOT 버튼**을 누르고 있다가 업로드가 시작되면 뗍니다.

포트가 안 보이면 USB 드라이버(CH340 또는 CP2102)를 설치하세요.

### 방법 B. PlatformIO (VS Code)
이 폴더(`esp32-bus-display`)를 열고 실행합니다.
```bash
pio run -t upload
```
```bash
pio device monitor
```

---

## 6. 하루 호출 한도 주의

개발계정 인증키는 **하루 호출 수가 제한**됩니다 (API마다 다르며 보통 1,000회 정도).

| 갱신 간격 | 하루 종일 켜 두면 | 05~24시만 켜면 |
|---|---|---|
| 60초 | 1,440회 ❌ | 1,140회 ❌ |
| **90초 (기본값)** | 960회 ✅ | 760회 ✅ |
| 120초 | 720회 ✅ | 570회 ✅ |

갱신 사이에도 화면의 남은 시간은 저절로 줄어들어서, 90초 간격으로도 충분히 쓸 만합니다. 더 자주 갱신하려면 data.go.kr에서 **운영계정**을 신청해 한도를 늘리세요.

---

## 7. 문제 해결

| 증상 | 해결 |
|---|---|
| 화면이 하얗거나 아무것도 안 나옴 | 배선을 연결표와 다시 비교 (특히 DC, CS, SCK, SDI). 화면 모듈의 VCC가 3V3에 꽂혀 있는지 확인 |
| 화면이 아예 어두움 | LED 선을 3V3에 꽂고 `TFT_BL -1` |
| 화면에 줄이 가거나 글자가 깨짐 | 점퍼선을 짧은 것으로 바꾸거나 `TFT_SPI_HZ`를 `20000000`으로 낮추기 |
| (CYD) 화면이 안 나옴 | USB 포트 2개짜리 CYD는 화면 칩이 ST7789라서 이 코드로는 안 될 수 있음 |
| 색이 반전되어 보임 | `TFT_INVERT_COLORS true` |
| 화면이 뒤집혀 보임 | `TFT_ROTATION`을 1 ↔ 3으로 변경 |
| "Wi-Fi 연결 실패" | 이름/비밀번호 확인. ESP32는 **2.4GHz만** 지원 (5GHz 안 됨) |
| "인증키 미등록" | 승인 후 1~2시간 기다리기. 그 API를 활용신청했는지 확인 |
| "오늘 호출 한도 초과" | `UPDATE_INTERVAL_SEC` 늘리기, `ACTIVE_START_HOUR`/`ACTIVE_END_HOUR`로 켜 두는 시간 줄이기 |
| 노선이 하나도 안 나옴 | 정류장 ID가 맞는지, `ROUTE_FILTER`의 노선 번호가 정확한지 확인 |
| 계속 재부팅됨 | 전원 부족. 다른 USB 어댑터나 짧은 케이블 사용 |

시리얼 모니터(115200)를 열면 요청 결과와 오류 응답의 앞부분이 출력되어 원인을 찾기 쉽습니다.

---

## 8. 파일 구성

| 파일 | 하는 일 |
|---|---|
| `BusArrivalDisplay/BusArrivalDisplay.ino` | 전체 흐름 (Wi-Fi, 갱신 주기, 화면 끄기) |
| `BusArrivalDisplay/bus_api.cpp` | API 주소 만들기, 응답 해석 (경기/서울/TAGO) |
| `BusArrivalDisplay/bus_fetch.cpp` | 인터넷으로 API 호출 |
| `BusArrivalDisplay/arrival_text.cpp` | "6분 (4정거장 전)" 같은 화면 문구 만들기 |
| `BusArrivalDisplay/display.cpp` | 화면 그리기 (한글 글꼴, 바뀐 줄만 다시 그림) |
| `BusArrivalDisplay/config.example.h` | 설정 예시 |
| `test_native/` | PC에서 응답 해석을 확인하는 테스트 |

### PC에서 테스트 실행
API 응답 해석과 화면 문구는 ESP32 없이 PC에서 확인할 수 있습니다.
```bash
cd test_native && make
```
