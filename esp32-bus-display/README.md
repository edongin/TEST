# 버스 도착 알림판 (Raspberry Pi Pico W / ESP32)

공공데이터포털의 **버스 도착정보 API**를 불러와서, ILI9341 화면(3.2인치/2.8인치)에 우리 집 앞 정류장의 버스 도착 시간을 보여줍니다.

기본 구성은 **SB Components "UHF Reader for Pico W" 보드 + ILI9341 3.2인치 터치 모듈**입니다. ESP32 개발보드나 CYD 보드에서도 같은 코드가 동작합니다.

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
- 밤에는 화면 끄기, 버튼으로 바로 새로고침 (Pico W 보드는 BT2, ESP32는 BOOT)
- Wi-Fi가 끊기거나 API가 실패하면 마지막 정보를 그대로 두고 아래에 경고만 표시

---

## 1. 준비물 (Pico W 구성)

| 부품 | 하는 일 | 비고 |
|---|---|---|
| **UHF Reader for Pico W** (SB Components, SKU26678) | **두뇌 + Wi‑Fi**. 보드에 Raspberry Pi Pico W가 붙어 있음 | RFID 기능은 이 프로젝트에서 쓰지 않고 꺼 둠 |
| **ILI9341 3.2인치 TFT LCD 모듈(터치)** | 버스 정보를 보여주는 화면 | 감압식 터치 (XPT2046) |
| 핀헤더 1x40 | 보드의 GPIO 구멍에 납땜 | 6핀씩 2조각만 씀 |
| 브레드보드 830핀 | 한 핀을 두 곳에 나눠 연결 | 170핀 미니도 가능 |
| 점퍼선 M/F, F/F | 연결 | |
| USB‑A ↔ Micro 5핀 케이블 | 프로그램 업로드 + 전원 | **데이터 전송이 되는 케이블**이어야 업로드 가능 |
| 휴대폰 충전기 (5V 1A 이상) | 상시 전원 | |

### 보드에서 쓸 수 있는 핀
UHF 리더 보드는 화면·RFID·SD카드·버튼이 이미 대부분의 핀을 쓰고 있어서, 밖으로 나온 핀은 아래 **두 줄(각 6개)**뿐입니다.

| 줄 | 핀 (보드에 적힌 이름) |
|---|---|
| GPIO 줄 1 | 5V, GP0, GP1, GP2, GP3, GND |
| GPIO 줄 2 | 3V3, GP21, GP20, GP28, GP15, GND |

이 8개 핀(GP0~3, GP15, GP20, GP21, GP28)으로 3.2인치 화면과 터치를 모두 연결합니다.

### 1‑1. 핀헤더 납땜
GPIO 줄 두 곳이 **구멍만 뚫려 있다면** 핀헤더를 납땜해야 합니다. (이미 핀이 꽂혀 있으면 넘어가세요.)

1. 1x40 핀헤더를 **6핀씩 두 조각** 잘라냅니다 (니퍼나 손으로 꺾으면 잘림).
2. 짧은 쪽을 보드 구멍에 끼우고 뒷면에서 납땜합니다. 모두 12군데입니다.
3. 납땜 인두가 없다면 가까운 메이커스페이스나 납땜 인두(1만~2만 원)를 이용하세요.

### 1‑2. 배선 (Pico W 배선표)
**USB를 뺀 상태에서** 연결하세요.

| 화면 모듈 핀 | 보드 핀 | 비고 |
|---|---|---|
| 1 VCC | **5V** (GPIO 줄 1) | |
| 2 GND | GND | |
| 3 CS | **GP1** | |
| 4 RESET | **GP20** | |
| 5 D/C | **GP21** | |
| 6 SDI (MOSI) | **GP3** | 터치 T_DIN과 나눔 |
| 7 SCK | **GP2** | 터치 T_CLK와 나눔 |
| 8 LED | **3.3V** (GPIO 줄 2) | 백라이트 항상 켜짐 |
| 9 SDO (MISO) | 연결 안 함 | 터치 신호와 부딪히지 않게 비워 둠 |
| 10 T_CLK | **GP2** | (나중에 터치용) |
| 11 T_CS | **GP15** | (나중에 터치용) |
| 12 T_DIN | **GP3** | (나중에 터치용) |
| 13 T_DO | **GP0** | (나중에 터치용) |
| 14 T_IRQ | **GP28** | (나중에 터치용) |

화면 모듈 위쪽의 SD카드용 4핀(J4)은 연결하지 않습니다.

**한 핀을 두 곳에 연결하는 법 (브레드보드)**
GP2와 GP3는 화면 모듈의 두 핀에 함께 연결해야 합니다.

```
보드 GP2 ──(M/F 선)──▶ 브레드보드 한 줄(예: 10번 줄) ◀──(M/F 선)── 화면 SCK
                                                  ◀──(M/F 선)── 화면 T_CLK
```

- 브레드보드는 **같은 번호 줄의 구멍 5개(a~e)가 서로 연결**되어 있습니다.
- M/F 선의 암(F)쪽을 보드나 화면의 핀에, 수(M)쪽을 브레드보드 구멍에 꽂습니다.
- GP3도 같은 방법으로 다른 줄을 만들어 화면 SDI와 T_DIN에 나눠 줍니다.
- 나머지 1:1 연결은 F/F 선으로 바로 꽂으면 됩니다.

> 백라이트(LED)가 3.3V에 바로 연결되어 있어서, 밤 시간에는 화면을 검게만 칠해 둡니다. 밤에 빛까지 완전히 끄고 싶으면 LED 선을 남는 GPIO에 연결하고 `config.h`의 `TFT_BL`에 그 번호를 적으면 됩니다.

### 1‑3. 전원
- **업로드할 때**: Pico W의 **Micro USB**를 PC에 연결합니다.
- **평소 사용**: Micro USB 또는 보드의 **USB‑C (PWR ONLY)** 단자를 휴대폰 충전기에 꽂아 둡니다.
- 보드에 배터리 단자와 충전 회로가 있어서, 리튬 배터리를 달면 잠깐 뽑아도 꺼지지 않게 할 수 있습니다 (선택).

<details>
<summary>ESP32 개발보드나 CYD 보드로 만들 때 (펼치기)</summary>

`config.h`의 `BOARD_CYD`로 고르고, Arduino IDE에서 보드를 `ESP32 Dev Module`로 선택합니다.

**ESP32 개발보드 + ILI9341 모듈 배선**

| 화면 핀 | ESP32 핀 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CS | GPIO 5 |
| RESET | GPIO 4 |
| DC | GPIO 26 |
| SDI (MOSI) | GPIO 23 |
| SCK | GPIO 18 |
| LED | GPIO 22 |
| SDO (MISO) | GPIO 19 |
| T_CLK / T_CS / T_DIN / T_DO / T_IRQ | GPIO 25 / 33 / 32 / 39(VN) / 36(VP) |

**CYD (ESP32-2432S028R)** 는 배선이 필요 없고 `BOARD_CYD 1`로만 바꾸면 됩니다.
</details>

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

> 보드에는 원래 MicroPython(RFID 예제용)이 들어 있습니다. 이 프로그램을 올리면 그것이 지워집니다.
> 되돌리고 싶으면 [제조사 펌웨어](https://github.com/sbcshop/UHF_Reader_Pico_W_Software)의 `uhfreader_picow_firmware.uf2`를 아래 BOOTSEL 방법으로 다시 넣으면 됩니다.

### 방법 A. Arduino IDE (처음이라면 추천)
1. [Arduino IDE](https://www.arduino.cc/en/software) 설치
2. **파일 → 기본 설정 → 추가 보드 관리자 URL**에 아래 주소를 넣습니다.
   ```
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   ```
3. **보드 매니저**에서 `Raspberry Pi Pico/RP2040/RP2350` (Earle F. Philhower, III) 설치
4. **라이브러리 매니저**에서 아래 3개를 설치합니다. 의존 라이브러리도 같이 설치하라고 하면 "모두 설치"를 누르세요.
   - `ArduinoJson` (Benoit Blanchon, 7.x)
   - `Adafruit ILI9341`
   - `U8g2_for_Adafruit_GFX`
5. `BusArrivalDisplay/BusArrivalDisplay.ino` 열기
6. **도구 → 보드**: `Raspberry Pi Pico W`
7. **처음 한 번은 BOOTSEL 모드로 연결합니다.**
   1. USB를 뺀 상태에서 Pico W 위의 흰색 **BOOTSEL** 버튼을 누른 채로
   2. Micro USB를 PC에 꽂고 버튼을 뗍니다.
   3. PC에 `RPI-RP2`라는 USB 드라이브가 보이면 성공입니다. (안 보이면 케이블이 충전 전용일 가능성이 큽니다.)
   4. **도구 → 포트**에서 `UF2 Board`를 고릅니다.
8. 업로드(→) 버튼을 누릅니다. 다음부터는 BOOTSEL 없이 일반 포트로 바로 업로드됩니다.

**ESP32로 만드는 경우**: 2번 주소 대신 `https://espressif.github.io/arduino-esp32/package_esp32_index.json`, 보드 매니저에서 `esp32` 설치, 보드는 `ESP32 Dev Module`, Partition Scheme은 `Huge APP`을 고릅니다.

### 방법 B. PlatformIO (VS Code)
이 폴더(`esp32-bus-display`)를 열고 실행합니다. 기본값은 Pico W이고, ESP32는 `-e esp32dev`를 붙입니다.
```bash
pio run -e pico_w -t upload
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
| 화면이 하얗거나 아무것도 안 나옴 | 배선을 연결표와 다시 비교 (특히 DC, CS, SCK, SDI). 화면 모듈의 VCC가 연결표대로 꽂혀 있는지 확인 |
| (Pico W) 화면이 계속 하얌 | CS(GP1), D/C(GP21), RESET(GP20), SCK(GP2), SDI(GP3) 연결을 다시 확인. 브레드보드에서 GP2·GP3가 같은 줄에 꽂혔는지 확인 |
| (Pico W) PC에 `RPI-RP2` 드라이브가 안 보임 | 데이터 전송이 되는 Micro USB 케이블로 바꾸기. BOOTSEL을 누른 채로 꽂았는지 확인 |
| (Pico W) 보드의 작은 화면이 켜져 있음 | 정상입니다. 이 프로그램이 시작되면 꺼집니다 |
| 화면이 아예 어두움 | LED 선을 3V3에 꽂고 `TFT_BL -1` |
| 화면에 줄이 가거나 글자가 깨짐 | 점퍼선을 짧은 것으로 바꾸거나 `TFT_SPI_HZ`를 `20000000`으로 낮추기 |
| (CYD) 화면이 안 나옴 | USB 포트 2개짜리 CYD는 화면 칩이 ST7789라서 이 코드로는 안 될 수 있음 |
| 색이 반전되어 보임 | `TFT_INVERT_COLORS true` |
| 화면이 뒤집혀 보임 | `TFT_ROTATION`을 1 ↔ 3으로 변경 |
| "Wi-Fi 연결 실패" | 이름/비밀번호 확인. Pico W와 ESP32 모두 **2.4GHz만** 지원 (5GHz 안 됨) |
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
| `BusArrivalDisplay/platform.cpp` | 보드마다 다른 부분 (Pico W / ESP32의 Wi‑Fi, 시계, UHF 보드 부품 끄기) |
| `BusArrivalDisplay/config.example.h` | 설정 예시 |
| `test_native/` | PC에서 응답 해석을 확인하는 테스트 |

### PC에서 테스트 실행
API 응답 해석과 화면 문구는 보드 없이 PC에서 확인할 수 있습니다.
```bash
cd test_native && make
```
