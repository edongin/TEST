// =============================================================================
//  화면·터치 테스트 (Raspberry Pi Pico WH + 3.2인치 SPI 터치 모듈, ILI9341 또는 ST7789)
//
//  버스 알림판을 올리기 전에 배선이 맞는지만 확인하는 프로그램입니다.
//  Wi-Fi, 인증키 설정이 필요 없습니다.
//
//  1단계: 색 확인   - 칸마다 적힌 색 이름과 실제 색이 맞는지 봅니다. (6초 또는 터치하면 다음)
//  2단계: 터치 확인 - 화면을 누르면 숫자가 바뀌고 점이 찍힙니다.
//  BOOTSEL 버튼   - 누를 때마다 백라이트 끄기/켜기 (LED 선 확인)
//
//  화면이 안 나올 때 보는 신호
//   - Pico 위의 작은 초록 LED: 켜지면 프로그램 시작, 0.5초마다 깜빡이면 정상으로 도는 중
//   - 시작할 때 화면 불빛이 3번 깜빡이면 화면 전원(VCC, GND)과 LED 선은 정상
// =============================================================================
// ----- 화면 칩 종류 -----
//   1 = ILI9341 (기본)
//   2 = ST7789  (ILI9341 로 아무것도 안 나오면 이걸로. IPS 화면에 많이 쓰임)
//       2 를 쓰려면 라이브러리 매니저에서 "Adafruit ST7735 and ST7789 Library" 를 설치하세요.
#define DISPLAY_DRIVER 1

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>  // 색 이름(ILI9341_RED 등)은 두 칩 모두 이걸 씀
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>
#if DISPLAY_DRIVER == 2
#include <Adafruit_ST7789.h>
#endif

// ----- 배선 (README 의 Pico WH 배선표와 같음) -----
#define TFT_SCLK  2   // SCK, T_CLK
#define TFT_MOSI  3   // SDI, T_DIN
#define TFT_CS    1
#define TFT_DC    21
#define TFT_RST   20
#define TFT_BL    22  // LED
#define TOUCH_CS  15  // T_CS
#define TOUCH_IRQ 28  // T_IRQ

// 터치 연결 방식 (버스 알림판 config.h 의 TOUCH_SEPARATE_PINS 와 같은 값으로)
//   0 = 브레드보드: T_CLK 는 GP2, T_DIN 은 GP3 (화면과 같은 핀), T_DO 는 GP0
//   1 = F/F 선 1:1 연결: T_CLK -> GP10, T_DIN -> GP11, T_DO -> GP12
#define TOUCH_SEPARATE_PINS 0

#if TOUCH_SEPARATE_PINS
#define TSPI SPI1  // 터치는 두 번째 SPI 통로
#else
#define TSPI SPI   // 터치가 화면과 같은 SPI 통로
#define TFT_MISO 0 // T_DO
#endif

// 1단계에서 색이 반대로 보이면 true ↔ false 를 바꾸세요. (3.2인치 IPS 모듈은 true 가 맞음)
// 여기서 맞는 값을 버스 알림판 config.h 의 TFT_INVERT_COLORS 에도 똑같이 넣으면 됩니다.
#define INVERT_COLORS true

// 화면 통신 속도. 점퍼선 연결에서도 안정적이도록 낮게 잡았습니다 (10MHz).
#define TFT_SPI_HZ 10000000

#if DISPLAY_DRIVER == 2
Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);  // ST7789 는 CS, DC 순서
#else
Adafruit_ILI9341 tft(&SPI, TFT_DC, TFT_CS, TFT_RST);
#endif
U8G2_FOR_ADAFRUIT_GFX u8f;

static void text(int16_t x, int16_t y, const char* s, uint16_t color) {
  u8f.setFont(u8g2_font_unifont_t_korean2);
  u8f.setFontMode(1);  // 글자 배경 투명 (글꼴을 바꾸면 다시 켜야 함)
  u8f.setForegroundColor(color);
  u8f.drawUTF8(x, y, s);
}

static void header(const char* title) {
  tft.fillRect(0, 0, 320, 28, tft.color565(20, 60, 140));
  text(8, 20, title, ILI9341_WHITE);
}

// ----- 터치 칩 (XPT2046) 읽기 -----
static const SPISettings TOUCH_SPI(2000000, MSBFIRST, SPI_MODE0);

static uint16_t touchRead(uint8_t cmd) {
  TSPI.transfer(cmd);
  uint16_t hi = TSPI.transfer(0), lo = TSPI.transfer(0);
  return (uint16_t)(((hi << 8) | lo) >> 3);
}

// 누르는 힘(z)과 위치(x, y) 원래 값
static void touchReadAll(int* x, int* y, int* z) {
  TSPI.beginTransaction(TOUCH_SPI);
  digitalWrite(TOUCH_CS, LOW);
  int z1 = touchRead(0xB1);
  int z2 = touchRead(0xC1);
  *x = touchRead(0xD1);
  *y = touchRead(0x91);
  touchRead(0x90);  // 절전 + 터치 알림(T_IRQ) 다시 켜기
  digitalWrite(TOUCH_CS, HIGH);
  TSPI.endTransaction();
  // 터치 칩이 대답하지 않으면 두 값이 똑같이 나옵니다 (선 빠짐, 핀 설정 다름).
  *z = (z1 == z2) ? 0 : z1 + 4095 - z2;
}

// ----- 1단계: 색 확인 -----
static void colorTest() {
  tft.fillScreen(ILI9341_BLACK);
  header("1단계: 색 확인");
  struct {
    const char* name;
    uint16_t color, label;
  } bars[] = {
      {"빨강", ILI9341_RED, ILI9341_WHITE},   {"초록", ILI9341_GREEN, ILI9341_BLACK},
      {"파랑", ILI9341_BLUE, ILI9341_WHITE},  {"흰색", ILI9341_WHITE, ILI9341_BLACK},
      {"노랑", ILI9341_YELLOW, ILI9341_BLACK},
  };
  for (int i = 0; i < 5; i++) {
    int16_t x = 8 + i * 62;
    tft.fillRect(x, 40, 56, 100, bars[i].color);
    text(x + 12, 96, bars[i].name, bars[i].label);
  }
  text(8, 170, "바탕이 검정이고 칸 이름과 색이", ILI9341_WHITE);
  text(8, 190, "같으면 정상입니다.", ILI9341_WHITE);
  text(8, 222, "다르면 INVERT_COLORS 를 바꾸세요", tft.color565(255, 150, 0));

  Serial.println("[1단계] 색 확인 화면 (6초 또는 터치하면 다음)");
  uint32_t start = millis();
  while (millis() - start < 6000) {
    if (digitalRead(TOUCH_IRQ) == LOW) break;
    delay(20);
  }
  while (digitalRead(TOUCH_IRQ) == LOW) delay(20);  // 손을 뗄 때까지
}

// ----- 2단계: 터치 확인 -----
static int tapCount = 0;
static char shown[4][64];  // 화면에 지금 쓰여 있는 상태 글자
static bool lastPressed = false;
static bool backlightOn = true;
static bool lastBootsel = false;

static void touchScreen() {
  tft.fillScreen(ILI9341_BLACK);
  memset(shown, 0, sizeof(shown));
  header("2단계: 터치 확인");
  text(8, 228, "BOOTSEL: 백라이트 끄기/켜기", tft.color565(150, 150, 150));
}

// 바뀐 줄만 다시 그립니다 (매번 전부 지우면 화면이 깜빡여 보임)

static void showLine(int i, const char* line, uint16_t color) {
  if (strcmp(shown[i], line) == 0) return;
  strncpy(shown[i], line, sizeof(shown[i]) - 1);
  int16_t baseline = 52 + i * 20;
  tft.fillRect(0, baseline - 16, 320, 20, ILI9341_BLACK);
  text(8, baseline, line, color);
}

static void showStatus(bool irq, bool pressed, int x, int y, int z) {
  char line[64];
  snprintf(line, sizeof(line), "T_IRQ 신호 : %s", irq ? "눌림" : "안 눌림");
  showLine(0, line, irq ? ILI9341_GREEN : ILI9341_WHITE);
  snprintf(line, sizeof(line), "누르는 힘  : %d", pressed ? z : 0);
  showLine(1, line, ILI9341_WHITE);
  if (pressed) {
    snprintf(line, sizeof(line), "위치 (원래값): %d, %d", x, y);
  } else {
    snprintf(line, sizeof(line), "위치 (원래값): -");
  }
  showLine(2, line, ILI9341_WHITE);
  snprintf(line, sizeof(line), "터치 횟수  : %d", tapCount);
  showLine(3, line, ILI9341_YELLOW);
}

void setup() {
  Serial.begin(115200);

  // [신호 1] Pico 위의 작은 초록 LED: 켜지면 프로그램이 시작된 것
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1500);  // 시리얼 모니터를 열 시간
  Serial.println("\n=== 화면·터치 테스트 ===");
  Serial.println("[1] 프로그램 시작 (Pico LED 켜짐)");

  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(TOUCH_IRQ, INPUT_PULLUP);

  // [신호 2] 화면 불빛 3번 깜빡이기: 화면 전원(VCC, GND)과 LED 선 확인
  Serial.println("[2] 화면 불빛 3번 깜빡이기");
  pinMode(TFT_BL, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(TFT_BL, HIGH);
    delay(400);
    digitalWrite(TFT_BL, LOW);
    delay(400);
  }
  digitalWrite(TFT_BL, HIGH);

  Serial.println("[3] 화면 초기화");
  SPI.setSCK(TFT_SCLK);
  SPI.setTX(TFT_MOSI);
#if TOUCH_SEPARATE_PINS
  SPI1.setSCK(10);  // T_CLK
  SPI1.setTX(11);   // T_DIN
  SPI1.setRX(12);   // T_DO
  SPI1.begin();
#else
  SPI.setRX(TFT_MISO);
#endif
#if DISPLAY_DRIVER == 2
  Serial.println("    화면 칩: ST7789");
  tft.init(240, 320);
  tft.setSPISpeed(TFT_SPI_HZ);
#else
  Serial.println("    화면 칩: ILI9341");
  tft.begin(TFT_SPI_HZ);
#endif
  tft.setRotation(1);
  // ST7789 라이브러리는 색 반전을 기본으로 켜 두므로 반대로 맞춥니다.
  tft.invertDisplay(DISPLAY_DRIVER == 2 ? !INVERT_COLORS : INVERT_COLORS);
  u8f.begin(tft);

  Serial.println("[4] 1단계 색 화면 그리기");
  colorTest();
  touchScreen();
  Serial.println("[5] 2단계: 화면을 눌러 보세요");
}

void loop() {
  static uint32_t lastShow = 0;
  static uint32_t lastBlink = 0;

  // Pico 위의 작은 LED 를 0.5초마다 깜빡임: 깜빡이면 프로그램이 멈추지 않고 도는 중
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }

  // BOOTSEL 로 백라이트 끄기/켜기
  bool bootsel = BOOTSEL;
  if (bootsel && !lastBootsel) {
    backlightOn = !backlightOn;
    digitalWrite(TFT_BL, backlightOn ? HIGH : LOW);
    Serial.printf("[BOOTSEL] 백라이트 %s\n", backlightOn ? "켬" : "끔");
  }
  lastBootsel = bootsel;

  // 터치 상태
  bool irq = digitalRead(TOUCH_IRQ) == LOW;
  int x, y, z;
  touchReadAll(&x, &y, &z);
  bool pressed = irq || z > 400;
  if (pressed && !lastPressed) {
    tapCount++;
    Serial.printf("[터치] %d번째 (T_IRQ=%s, 힘=%d, 위치=%d,%d)\n", tapCount, irq ? "LOW" : "HIGH",
                  z, x, y);
  }
  lastPressed = pressed;

  // 누르는 곳에 점 찍기 (위치가 손가락과 어긋나거나 뒤집혀도 지금은 괜찮습니다)
  if (z > 400) {
    int sx = map(constrain(x, 200, 3900), 200, 3900, 0, 319);
    int sy = map(constrain(y, 200, 3900), 200, 3900, 120, 215);
    tft.fillCircle(sx, sy, 3, ILI9341_CYAN);
  }

  if (millis() - lastShow > 150) {
    lastShow = millis();
    showStatus(irq, pressed, x, y, z);
  }
  delay(20);
}
