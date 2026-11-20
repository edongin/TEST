#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Arduino.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "app_config.h"
#include "arrival_text.h"
#include <math.h>
#include "platform.h"

// ----- 화면 배치 (가로 320 x 세로 240) -----
static const int16_t W = 320;
static const int16_t H = 240;
static const int16_t HEADER_H = 30;
static const int16_t FOOTER_H = 20;
static const int16_t ROUTE_W = 96;  // 왼쪽 노선 번호 칸 너비
static const int16_t TEXT_X = ROUTE_W + 6;
static const int16_t TEXT_W = W - TEXT_X - 6;

// 한글이 들어가는 글꼴 (16픽셀, 자주 쓰는 한글 2,350자 포함)
#define FONT_KOREAN u8g2_font_unifont_t_korean2
// 노선 번호용 큰 영문/숫자 글꼴
#define FONT_ROUTE_BIG u8g2_font_helvB18_tf
#define FONT_ROUTE_MID u8g2_font_helvB14_tf

// 화면이 쓰는 SPI 통로
#if defined(ARDUINO_ARCH_RP2040)
// Pico W: SPI0 (GP0~GP3). 나중에 터치도 같은 통로를 함께 씁니다.
#define tftSpi SPI
#elif TFT_USE_HSPI && defined(HSPI)  // ESP32: 다른 통로는 나중에 터치용으로 남겨 둠
static SPIClass tftSpi(HSPI);
#elif defined(VSPI)
static SPIClass tftSpi(VSPI);
#else
static SPIClass tftSpi(FSPI);
#endif
static Adafruit_ILI9341 tft(&tftSpi, TFT_DC, TFT_CS, TFT_RST);
static U8G2_FOR_ADAFRUIT_GFX u8f;
static GFXcanvas1 bigCanvas(160, 18);  // 한글 2배 글자용 그림판 (1배 160픽셀 = 화면 320픽셀)
static U8G2_FOR_ADAFRUIT_GFX u8big;

// 글꼴 바꾸기. 이 라이브러리는 글꼴을 바꿀 때마다 "글자 배경 칠하기"로 되돌아가서
// 매번 투명 모드(1)를 다시 켜 줍니다. (안 하면 글자 뒤에 검은 상자가 생김)
static void useFont(const uint8_t* font) {
  u8f.setFont(font);
  u8f.setFontMode(1);
}

static uint16_t C_BG, C_ROW_ALT, C_HEADER, C_TEXT, C_DIM, C_ROUTE, C_SOON, C_WARN;
static uint16_t C_SUN, C_CLOUD, C_DARK_CLOUD, C_RAIN, C_SNOW, C_COLD, C_HOT;
static uint16_t C_TEXT2;  // 조금 덜 밝은 흰색 (정거장 수 등)

// 마지막으로 그린 내용을 기억해서, 바뀐 곳만 다시 그립니다.
static char headerCache[96];
static char rowCache[ROWS_PER_PAGE][200];
static char footerCache[160];
static char bodyCache[96];  // 날씨 화면 본문

static void clearCaches() {
  headerCache[0] = '\0';
  footerCache[0] = '\0';
  bodyCache[0] = '\0';
  for (auto& row : rowCache) row[0] = '\0';
}

// 바뀌었으면 true 를 돌려주고 기억해 둡니다.
static bool changed(char* cache, size_t size, const char* key) {
  if (strncmp(cache, key, size) == 0) return false;
  bus::copyUtf8(cache, size, key);
  return true;
}

// ---------------------------------------------------------------------------
// 글자 그리기 도우미
// ---------------------------------------------------------------------------

static void drawText(int16_t x, int16_t baseline, const char* text, uint16_t color) {
  u8f.setForegroundColor(color);
  u8f.drawUTF8(x, baseline, text);
}

// 너비가 넘치면 뒤를 잘라 ".." 을 붙입니다.
static void drawClipped(int16_t x, int16_t baseline, int16_t maxW, const char* text,
                        uint16_t color) {
  if (u8f.getUTF8Width(text) <= maxW) {
    drawText(x, baseline, text, color);
    return;
  }
  char buf[128];
  bus::copyUtf8(buf, sizeof(buf), text);
  const int16_t dotsW = u8f.getUTF8Width("..");
  size_t len = strlen(buf);
  while (len > 0) {
    // 한 글자(UTF-8 1~3바이트)씩 뒤에서 지웁니다.
    do {
      len--;
    } while (len > 0 && ((unsigned char)buf[len] & 0xC0) == 0x80);
    buf[len] = '\0';
    if (u8f.getUTF8Width(buf) + dotsW <= maxW) break;
  }
  strncat(buf, "..", sizeof(buf) - strlen(buf) - 1);
  drawText(x, baseline, buf, color);
}

static void drawCentered(int16_t centerY, const char* text, uint16_t color) {
  int16_t w = u8f.getUTF8Width(text);
  int16_t x = w < W ? (W - w) / 2 : 0;
  drawClipped(x, centerY + u8f.getFontAscent() / 2, W, text, color);
}

static bool isAscii(const char* s) {
  for (; *s; s++) {
    if ((unsigned char)*s >= 0x80) return false;
  }
  return true;
}

// ---------------------------------------------------------------------------

void displayBegin() {
  if (TFT_BL >= 0) {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
  }
#if defined(ARDUINO_ARCH_RP2040)
  // 핀만 정해 두면 tft.begin() 안에서 SPI 가 시작됩니다. CS 는 라이브러리가 직접 제어.
  tftSpi.setSCK(TFT_SCLK);
  tftSpi.setTX(TFT_MOSI);
  if (TFT_MISO >= 0) tftSpi.setRX(TFT_MISO);
#else
  tftSpi.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
#endif
  tft.begin(TFT_SPI_HZ);
  tft.setRotation(TFT_ROTATION);
  tft.invertDisplay(TFT_INVERT_COLORS);

  u8f.begin(tft);
  u8big.begin(bigCanvas);
  u8f.setFontDirection(0);

  C_BG = ILI9341_BLACK;
  C_ROW_ALT = tft.color565(18, 22, 30);
  C_HEADER = tft.color565(20, 60, 140);
  C_TEXT = ILI9341_WHITE;
  C_DIM = tft.color565(160, 160, 160);
  C_TEXT2 = tft.color565(215, 215, 215);
  C_ROUTE = tft.color565(90, 210, 255);
  C_SOON = tft.color565(255, 150, 0);
  C_WARN = tft.color565(255, 80, 80);
  C_SUN = tft.color565(255, 200, 0);
  C_CLOUD = tft.color565(225, 228, 235);
  C_DARK_CLOUD = tft.color565(140, 145, 155);
  C_RAIN = tft.color565(80, 160, 255);
  C_SNOW = ILI9341_WHITE;
  C_COLD = tft.color565(110, 180, 255);
  C_HOT = tft.color565(255, 120, 90);

  tft.fillScreen(C_BG);
  clearCaches();
}

void displaySetSleep(bool sleep) {
  if (sleep) {
    tft.fillScreen(C_BG);
    // 백라이트를 끌 수 없으면 화면만 검게 칠해 둡니다.
    // (백라이트가 켜진 채 DISPOFF 를 보내면 화면이 하얗게 보임)
    if (TFT_BL >= 0) {
      digitalWrite(TFT_BL, LOW);
      tft.sendCommand(ILI9341_DISPOFF);
      tft.sendCommand(ILI9341_SLPIN);  // 화면 칩도 절전
      delay(5);
    }
  } else {
    if (TFT_BL >= 0) {
      tft.sendCommand(ILI9341_SLPOUT);
      delay(120);  // 절전에서 깨어나는 데 필요한 시간
      tft.sendCommand(ILI9341_DISPON);
    }
    tft.fillScreen(C_BG);
    if (TFT_BL >= 0) digitalWrite(TFT_BL, HIGH);
  }
  clearCaches();
}

void displayClear() {
  tft.fillScreen(C_BG);
  clearCaches();
}

static void drawHeader(const char* station);

void displayMessage(const char* title, const char* line1, const char* line2) {
  tft.fillScreen(C_BG);
  clearCaches();
  drawHeader(title);  // 제목 + 시계
  useFont(FONT_KOREAN);
  if (line1) drawCentered(H / 2 - 14, line1, C_TEXT);
  if (line2) drawCentered(H / 2 + 14, line2, C_TEXT2);
}

void displayHeader(const char* title) { drawHeader(title); }

// ---------------------------------------------------------------------------

static void drawHeader(const char* station) {
  char clock[8] = "";
  struct tm now;
  if (platformLocalTime(&now)) strftime(clock, sizeof(clock), "%H:%M", &now);

  char key[sizeof(headerCache)];
  snprintf(key, sizeof(key), "%s|%s", station, clock);
  if (!changed(headerCache, sizeof(headerCache), key)) return;

  tft.fillRect(0, 0, W, HEADER_H, C_HEADER);
  useFont(FONT_KOREAN);
  int16_t clockW = clock[0] ? u8f.getUTF8Width(clock) : 0;
  drawClipped(8, 21, W - 24 - clockW, station, C_TEXT);
  if (clock[0]) drawText(W - 8 - clockW, 21, clock, C_TEXT);
}

// ---------------------------------------------------------------------------
// 한글 2배 크기 글자
// 한글 글꼴은 16픽셀 하나뿐이라, 작은 흑백 그림판에 먼저 그린 뒤 2배로 키워서 옮깁니다.
// ---------------------------------------------------------------------------

static void useBigFont() {
  u8big.setFont(FONT_KOREAN);
  u8big.setFontMode(1);
}

static int16_t bigWidth(const char* text) {
  useBigFont();
  return u8big.getUTF8Width(text) * 2;
}

// 한글 글자는 글꼴이 알려 주는 높이(영문 기준)보다 위로 더 올라가서, 바닥선을 넉넉히 잡습니다.
static const int16_t BIG_BASE = 15;  // 그림판 안의 바닥선 (16픽셀 글꼴, 아래로 2픽셀)

static int16_t bigAscent() { return 14 * 2; }

// baseline: 2배 글자의 바닥선 위치
static void drawBig(int16_t x, int16_t baseline, const char* text, uint16_t color) {
  useBigFont();
  u8big.setForegroundColor(1);
  bigCanvas.fillScreen(0);
  u8big.drawUTF8(0, BIG_BASE, text);
  int16_t w = u8big.getUTF8Width(text);
  if (w > bigCanvas.width()) w = bigCanvas.width();
  const int16_t top = baseline - BIG_BASE * 2;
  // 가로로 이어진 점들을 한 번에 칠해서 빠르게 그립니다.
  for (int16_t y = 0; y < bigCanvas.height(); y++) {
    int16_t run = -1;
    for (int16_t px = 0; px <= w; px++) {
      bool on = px < w && bigCanvas.getPixel(px, y);
      if (on && run < 0) {
        run = px;
      } else if (!on && run >= 0) {
        tft.fillRect(x + run * 2, top + y * 2, (px - run) * 2, 2, color);
        run = -1;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// 정류장 화면
// 버스가 적으면 줄을 크게: 1대 = 화면 전체, 2대 = 2줄, 3대 = 3줄, 4대 이상 = 5줄
// ---------------------------------------------------------------------------

#define FONT_ROUTE_HUGE  u8g2_font_logisoso42_tr  // 2줄일 때 노선 번호
#define FONT_ROUTE_LARGE u8g2_font_logisoso32_tr  // 3줄일 때 노선 번호
#define FONT_ROUTE_24    u8g2_font_helvB24_tf

struct RowLayout {
  int rows;   // 한 화면에 보일 줄 수
  int16_t h;  // 한 줄 높이
  bool big;   // 큰 글씨 배치인지
};

static RowLayout layoutFor(int count) {
  const int16_t body = H - HEADER_H - FOOTER_H;  // 190
  if (count >= 4) return {ROWS_PER_PAGE, (int16_t)(body / ROWS_PER_PAGE), false};
  if (count == 3) return {3, (int16_t)(body / 3), true};
  if (count == 2) return {2, (int16_t)(body / 2), true};
  return {1, body, true};  // 1대면 화면 전체를 한 줄로
}

static const int16_t BIG_ROUTE_W = 118;  // 큰 배치에서 왼쪽 노선 번호 칸
static const int16_t BIG_TEXT_X = BIG_ROUTE_W + 8;
static const int16_t BIG_TEXT_W = W - BIG_TEXT_X - 6;

static void drawRouteName(int16_t y0, int16_t h, bool big, const char* name) {
  const int16_t colW = big ? BIG_ROUTE_W : ROUTE_W;
  if (!isAscii(name)) {
    // 한글이 들어간 노선 번호 (예: "마을02")
    if (big && bigWidth(name) <= colW - 8) {
      drawBig(8, y0 + (h + bigAscent()) / 2, name, C_ROUTE);
    } else {
      useFont(FONT_KOREAN);
      drawClipped(8, y0 + (h + u8f.getFontAscent()) / 2, colW - 8, name, C_ROUTE);
    }
    return;
  }
  // 영문/숫자 노선 번호: 칸에 들어가는 가장 큰 글꼴
  // 큰 글꼴부터 한 단계씩 줄여 가며 칸에 맞는 것을 고릅니다.
  const uint8_t* fonts[] = {FONT_ROUTE_HUGE, FONT_ROUTE_LARGE, FONT_ROUTE_24, FONT_ROUTE_BIG,
                            FONT_ROUTE_MID};
  const int count = sizeof(fonts) / sizeof(fonts[0]);
  int first = !big ? 3 : (h >= 90 ? 0 : 1);
  for (int i = first; i < count; i++) {
    useFont(fonts[i]);
    if (u8f.getUTF8Width(name) <= colW - 8 || i == count - 1) break;
  }
  int16_t baseline = y0 + (h + u8f.getFontAscent()) / 2;
  drawClipped(8, baseline, colW - 8, name, C_ROUTE);
}

// "다음 15분" 은 흰색, " / 범계역 방면" 은 회색으로 이어서 씁니다.
static void drawNextLine(int16_t x, int16_t baseline, int16_t maxW, const char* second,
                         const char* dest) {
  useFont(FONT_KOREAN);
  int16_t used = 0;
  if (second[0]) {
    drawClipped(x, baseline, maxW, second, C_TEXT);
    used = u8f.getUTF8Width(second);
  }
  if (dest[0] && used < maxW - 24) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%s", second[0] ? " / " : "", dest);
    drawClipped(x + used, baseline, maxW - used, buf, C_TEXT2);
  }
}

static void drawRow(int index, const RowLayout& lay, const bus::Route* route, uint32_t ageSec) {
  char mainText[48] = "", detail[48] = "", second[48] = "", first[64] = "";
  bus::Urgency urgency = bus::URGENCY_NONE;
  if (route) {
    if (lay.big) {
      urgency = bus::formatMain(route->next[0], ageSec, mainText, sizeof(mainText));
      bus::formatDetail(route->next[0], detail, sizeof(detail));
    } else {
      urgency = bus::formatFirst(route->next[0], ageSec, first, sizeof(first));
    }
    bus::formatSecond(route->next[1], ageSec, second, sizeof(second));
  }

  char key[sizeof(rowCache[0])];
  snprintf(key, sizeof(key), "%d|%s|%s%s|%s|%s|%d", lay.rows, route ? route->name : "", mainText,
           first, detail, second, urgency);
  if (!changed(rowCache[index], sizeof(rowCache[index]), key)) return;

  const int16_t y0 = HEADER_H + index * lay.h;
  tft.fillRect(0, y0, W, lay.h, (index % 2) ? C_ROW_ALT : C_BG);
  if (!route) return;

  drawRouteName(y0, lay.h, lay.big, route->name);
  // 도착 시간은 흰색. 정보가 없을 때만 회색.
  const uint16_t timeColor = urgency == bus::URGENCY_NONE ? C_DIM : C_TEXT;

  if (!lay.big) {
    // 4대 이상: 한 줄에 작게
    useFont(FONT_KOREAN);
    drawClipped(TEXT_X, y0 + 17, TEXT_W, first, timeColor);
    drawNextLine(TEXT_X, y0 + 34, TEXT_W, second, route->dest);
    return;
  }

  // 큰 배치: 도착 시간을 2배 크기로. 줄이 아주 높으면(1대) 글자 묶음을 가운데로.
  const int16_t cy = lay.h > 95 ? y0 + (lay.h - 95) / 2 : y0;
  const int16_t mainBase = lay.h >= 90 ? cy + 42 : y0 + 37;
  int16_t mainW = bigWidth(mainText);
  if (mainW <= BIG_TEXT_W) {
    drawBig(BIG_TEXT_X, mainBase, mainText, timeColor);
  } else {
    useFont(FONT_KOREAN);
    mainW = u8f.getUTF8Width(mainText);
    drawClipped(BIG_TEXT_X, mainBase, BIG_TEXT_W, mainText, timeColor);
  }

  useFont(FONT_KOREAN);
  const int16_t detailW = detail[0] ? u8f.getUTF8Width(detail) : 0;
  if (lay.h >= 90) {
    // 2줄 배치: 시간 / 정거장 수 / 다음 버스와 방면
    if (detail[0]) drawClipped(BIG_TEXT_X, cy + 64, BIG_TEXT_W, detail, C_TEXT2);
    drawNextLine(BIG_TEXT_X, cy + 86, BIG_TEXT_W, second, route->dest);
  } else if (detail[0] && BIG_TEXT_X + mainW + 10 + detailW <= W - 6) {
    // 3줄 배치: 시간 옆에 정거장 수, 아래에 다음 버스와 방면
    drawText(BIG_TEXT_X + mainW + 10, mainBase, detail, C_TEXT2);
    drawNextLine(BIG_TEXT_X, y0 + 57, BIG_TEXT_W, second, route->dest);
  } else {
    // 3줄 배치인데 옆에 자리가 없으면: 정거장 수와 다음 버스를 아래 줄에
    char line[96];
    snprintf(line, sizeof(line), "%s%s%s", detail, detail[0] && second[0] ? "  " : "", second);
    drawClipped(BIG_TEXT_X, y0 + 57, BIG_TEXT_W, line, C_TEXT2);
  }
}

static void drawEmpty() {
  const char* key = "<empty>";
  if (!changed(rowCache[0], sizeof(rowCache[0]), key)) return;
  for (int i = 1; i < ROWS_PER_PAGE; i++) rowCache[i][0] = '\0';
  tft.fillRect(0, HEADER_H, W, H - HEADER_H - FOOTER_H, C_BG);
  useFont(FONT_KOREAN);
  drawCentered(HEADER_H + (H - HEADER_H - FOOTER_H) / 2, "지금 도착 예정인 버스가 없어요", C_TEXT2);
}

// 맨 아래 줄: 왼쪽에 안내(또는 빨간 경고), 오른쪽에 짧은 글 (페이지 번호 등)
static void drawFooter(const char* footer, const char* warning, const char* pageText) {
  bool warn = warning && warning[0];

  char key[sizeof(footerCache)];
  snprintf(key, sizeof(key), "%s|%s|%s", warn ? warning : footer, pageText, warn ? "!" : "");
  if (!changed(footerCache, sizeof(footerCache), key)) return;

  int16_t y0 = H - FOOTER_H;
  tft.fillRect(0, y0, W, FOOTER_H, C_BG);
  tft.drawFastHLine(0, y0, W, C_ROW_ALT);
  useFont(FONT_KOREAN);
  int16_t pageW = pageText[0] ? u8f.getUTF8Width(pageText) : 0;
  drawClipped(8, H - 4, W - 24 - pageW, warn ? warning : footer, warn ? C_WARN : C_TEXT2);
  if (pageText[0]) drawText(W - 8 - pageW, H - 4, pageText, C_TEXT2);
}

void displayArrivals(const bus::Result& data, uint32_t ageSec, const char* station,
                     const char* footer, const char* warning, int page) {
  static int lastRows = -1;
  const RowLayout lay = layoutFor(data.count);
  int pages = (data.count + lay.rows - 1) / lay.rows;
  if (pages < 1) pages = 1;
  page %= pages;

  drawHeader(station);
  if (data.count == 0) {
    drawEmpty();
    lastRows = -1;
  } else {
    if (lay.rows != lastRows || strcmp(rowCache[0], "<empty>") == 0) {
      // 줄 수가 바뀌면 본문을 지우고 처음부터 다시 그림
      tft.fillRect(0, HEADER_H, W, H - HEADER_H - FOOTER_H, C_BG);
      for (auto& row : rowCache) row[0] = '\0';
      lastRows = lay.rows;
    }
    for (int i = 0; i < lay.rows; i++) {
      int idx = page * lay.rows + i;
      drawRow(i, lay, idx < data.count ? &data.routes[idx] : nullptr, ageSec);
    }
  }
  char pageText[12] = "";
  if (pages > 1) snprintf(pageText, sizeof(pageText), "%d/%d", page + 1, pages);
  drawFooter(footer, warning, pageText);
}

// ===========================================================================
// 날씨 화면
// ===========================================================================

#define FONT_BIG_NUM u8g2_font_logisoso32_tr  // 큰 기온 숫자

static int roundTemp(int16_t t10) { return t10 >= 0 ? (t10 + 5) / 10 : -((-t10 + 5) / 10); }

// ----- 날씨 아이콘 (도형으로 직접 그림). s = 아이콘 크기(픽셀) -----

static void drawCloud(int16_t cx, int16_t cy, int16_t s, uint16_t color) {
  tft.fillCircle(cx - s * 18 / 100, cy + s * 4 / 100, s * 17 / 100, color);
  tft.fillCircle(cx + s * 4 / 100, cy - s * 8 / 100, s * 23 / 100, color);
  tft.fillCircle(cx + s * 24 / 100, cy + s * 6 / 100, s * 15 / 100, color);
  int16_t h = s * 18 / 100;
  tft.fillRoundRect(cx - s * 35 / 100, cy + s * 4 / 100, s * 74 / 100, h, h / 2, color);
}

static void drawSun(int16_t cx, int16_t cy, int16_t r) {
  tft.fillCircle(cx, cy, r, C_SUN);
  for (int i = 0; i < 8; i++) {
    float a = i * 3.14159265f / 4;
    int16_t x0 = cx + (int16_t)(cosf(a) * (r + 3 + r / 4));
    int16_t y0 = cy + (int16_t)(sinf(a) * (r + 3 + r / 4));
    int16_t x1 = cx + (int16_t)(cosf(a) * (r * 2 + 2));
    int16_t y1 = cy + (int16_t)(sinf(a) * (r * 2 + 2));
    tft.drawLine(x0, y0, x1, y1, C_SUN);
    if (r >= 8) tft.drawLine(x0 + 1, y0, x1 + 1, y1, C_SUN);
  }
}

// 구름 아래 빗줄기(l), 눈송이(o) 를 pattern 순서대로
static void drawPrecip(int16_t cx, int16_t top, int16_t s, const char* pattern) {
  int16_t step = s * 20 / 100, len = s * 18 / 100;
  int n = (int)strlen(pattern);
  int16_t x = cx - step * (n - 1) / 2;
  for (int i = 0; i < n; i++, x += step) {
    if (pattern[i] == 'l') {
      int16_t dx = len / 3;
      tft.drawLine(x + dx, top, x - dx, top + len, C_RAIN);
      tft.drawLine(x + dx + 1, top, x - dx + 1, top + len, C_RAIN);
    } else {
      int16_t r = s >= 40 ? 3 : 2;
      tft.fillCircle(x, top + len / 2, r, C_SNOW);
    }
  }
}

static void drawWeatherIcon(int16_t cx, int16_t cy, int16_t s, weather::Condition c) {
  using namespace weather;
  const int16_t up = s * 12 / 100;  // 비·눈일 때 구름을 위로 올리는 정도
  const int16_t dropTop = cy + s * 20 / 100;
  switch (c) {
    case COND_CLEAR:
      drawSun(cx, cy, s * 20 / 100);
      break;
    case COND_PARTLY:
      drawSun(cx - s * 16 / 100, cy - s * 14 / 100, s * 15 / 100);
      drawCloud(cx + s * 8 / 100, cy + s * 10 / 100, s * 78 / 100, C_CLOUD);
      break;
    case COND_CLOUDY:
      drawCloud(cx - s * 10 / 100, cy - s * 10 / 100, s * 68 / 100, C_DARK_CLOUD);
      drawCloud(cx + s * 6 / 100, cy + s * 8 / 100, s * 84 / 100, C_CLOUD);
      break;
    case COND_RAIN:
      drawCloud(cx, cy - up, s, C_DARK_CLOUD);
      drawPrecip(cx, dropTop, s, "lll");
      break;
    case COND_SHOWER:
      drawSun(cx + s * 22 / 100, cy - s * 26 / 100, s * 12 / 100);
      drawCloud(cx - s * 4 / 100, cy - up, s * 90 / 100, C_DARK_CLOUD);
      drawPrecip(cx, dropTop, s, "ll");
      break;
    case COND_SNOW:
      drawCloud(cx, cy - up, s, C_CLOUD);
      drawPrecip(cx, dropTop, s, "ooo");
      break;
    case COND_RAIN_SNOW:
      drawCloud(cx, cy - up, s, C_DARK_CLOUD);
      drawPrecip(cx, dropTop, s, "lol");
      break;
    default:
      useFont(FONT_KOREAN);
      drawText(cx - 4, cy + 6, "?", C_DIM);
      break;
  }
}

// ----- 글자 -----

static void drawCenteredAt(int16_t cx, int16_t baseline, const char* text, uint16_t color) {
  int16_t w = u8f.getUTF8Width(text);
  drawText(cx - w / 2, baseline, text, color);
}

// 큰 기온 숫자 + 도(°) 표시. 그린 너비를 돌려줍니다.
static int16_t bigTempWidth(int temp, const uint8_t* font) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", temp);
  useFont(font);
  return u8f.getUTF8Width(buf) + 14;
}

static int16_t drawBigTemp(int16_t x, int16_t baseline, int temp, uint16_t color,
                           const uint8_t* font = FONT_BIG_NUM) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", temp);
  useFont(font);
  drawText(x, baseline, buf, color);
  int16_t w = u8f.getUTF8Width(buf);
  int16_t top = baseline - u8f.getFontAscent();
  tft.drawCircle(x + w + 7, top + 5, 4, color);
  tft.drawCircle(x + w + 7, top + 5, 3, color);
  return w + 14;
}

// "최저 / 최고" 를 maxW 안에 들어가는 가장 큰 글꼴로 (파랑 / 빨강)
static void drawMinMax(int16_t x, int16_t baseline, int16_t maxW, int tmin, int tmax) {
  const uint8_t* fonts[] = {u8g2_font_logisoso42_tr, u8g2_font_logisoso32_tr,
                            u8g2_font_logisoso28_tr, u8g2_font_logisoso24_tr};
  const uint8_t* font = fonts[3];
  for (const uint8_t* f : fonts) {
    useFont(f);
    int16_t slashW = u8f.getUTF8Width("/") + 12;
    if (bigTempWidth(tmin, f) + slashW + bigTempWidth(tmax, f) <= maxW) {
      font = f;
      break;
    }
  }
  x += drawBigTemp(x, baseline, tmin, C_COLD, font);
  useFont(font);
  drawText(x + 2, baseline, "/", C_TEXT);
  x += u8f.getUTF8Width("/") + 12;
  drawBigTemp(x, baseline, tmax, C_HOT, font);
}

// 오전/오후 줄은 크게 쓰려고 짧은 이름을 씁니다 ("구름많음" -> "구름")
static const char* shortConditionName(weather::Condition c) {
  return c == weather::COND_PARTLY ? "구름" : weather::conditionName(c);
}

// "오전 맑음 / 오후 비" 를 가운데에. 들어가면 2배 크기, 안 들어가면 보통 크기.
static void drawAmPm(int16_t baseline2x, weather::Condition am, weather::Condition pm) {
  char line[64];
  snprintf(line, sizeof(line), "오전 %s / 오후 %s", shortConditionName(am), shortConditionName(pm));
  int16_t w2 = bigWidth(line);
  if (w2 <= W - 12) {
    drawBig((W - w2) / 2, baseline2x, line, C_TEXT);
  } else {
    useFont(FONT_KOREAN);
    drawCentered(baseline2x - 12, line, C_TEXT);
  }
}

static void popText(char* out, size_t size, int am, int pm) {
  char a[8] = "-", p[8] = "-";
  if (am >= 0) snprintf(a, sizeof(a), "%d%%", am);
  if (pm >= 0) snprintf(p, sizeof(p), "%d%%", pm);
  snprintf(out, size, "강수확률  오전 %s   오후 %s", a, p);
}

// 시간별 예보 한 칸 (5칸 중 col 번째)
static void drawHourSlot(int col, int hour, const weather::Hour* h) {
  const int16_t colW = W / 5, cx = col * colW + colW / 2;
  const int16_t y0 = 164;
  if (!h) return;
  char buf[16];
  useFont(FONT_KOREAN);
  snprintf(buf, sizeof(buf), "%02d시", hour);
  drawCenteredAt(cx, y0 + 13, buf, C_TEXT2);
  drawWeatherIcon(cx, y0 + 29, 24, weather::hourCondition(*h));
  if (h->temp != weather::UNKNOWN) {
    snprintf(buf, sizeof(buf), "%d도", h->temp);
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  useFont(FONT_KOREAN);
  drawCenteredAt(cx, y0 + 53, buf, C_TEXT);
}

static void drawWeatherBody(const WeatherData& w, int nowHour) {
  using namespace weather;
  const Day& day = w.day;
  tft.fillRect(0, HEADER_H, W, H - HEADER_H - FOOTER_H, C_BG);

  int popAm, popPm;
  Condition am = summarize(day, 6, 12, &popAm);
  Condition pm = summarize(day, 12, 19, &popPm);
  const bool haveMinMax = day.tmin10 != UNKNOWN10 && day.tmax10 != UNKNOWN10;
  char line[96];

  // ----- 윗줄: 날씨 그림 / (오늘) 지금 기온 / 최저·최고 -----
  if (w.tomorrow) {
    int dummy;
    drawWeatherIcon(40, 70, 58, summarize(day, 6, 19, &dummy));
    useFont(FONT_KOREAN);
    drawText(92, 50, "최저 / 최고", C_TEXT);
    if (haveMinMax) drawMinMax(92, 101, W - 92 - 6, roundTemp(day.tmin10), roundTemp(day.tmax10));
  } else {
    int8_t sky = (nowHour >= 0 && nowHour < 24) ? day.hours[nowHour].sky : UNKNOWN;
    Condition current;
    if (w.now.ok) {
      current = nowCondition(w.now, sky);
    } else {
      int dummy;
      current = summarize(day, nowHour, nowHour + 3, &dummy);
    }
    if (current == COND_UNKNOWN) current = am != COND_UNKNOWN ? am : pm;
    drawWeatherIcon(40, 70, 58, current);

    // 지금 기온
    useFont(FONT_KOREAN);
    drawText(80, 50, "지금", C_TEXT);
    if (w.now.ok) {
      drawBigTemp(80, 101, roundTemp(w.now.temp10), C_TEXT, u8g2_font_logisoso42_tr);
    } else if (nowHour >= 0 && nowHour < 24 && day.hours[nowHour].temp != UNKNOWN) {
      drawBigTemp(80, 101, day.hours[nowHour].temp, C_TEXT, u8g2_font_logisoso42_tr);
    }

    // 최저 / 최고
    const int16_t mx = 182;
    useFont(FONT_KOREAN);
    drawText(mx, 50, "최저 / 최고", C_TEXT);
    if (haveMinMax) drawMinMax(mx, 101, W - mx - 4, roundTemp(day.tmin10), roundTemp(day.tmax10));
  }

  // ----- 가운데: 오전 / 오후 날씨 (크게) -----
  drawAmPm(134, am, pm);

  // ----- 강수확률 -----
  useFont(FONT_KOREAN);
  popText(line, sizeof(line), popAm, popPm);
  drawCentered(150, line, C_TEXT);
  tft.drawFastHLine(8, 162, W - 16, C_ROW_ALT);

  // ----- 시간별 예보 5칸 (3시간 간격). 오늘은 다음 시각부터, 내일은 06시부터 -----
  int first = w.tomorrow ? 6 : nowHour + 1;
  if (first < 0) first = 0;
  for (int col = 0; col < 5; col++) {
    int hour = first + col * 3;
    if (hour > 23) break;
    // 그 시각 예보가 없으면 (예: 02시 발표에는 00~02시가 없음) 바로 다음 시각 것
    const Hour* h = &day.hours[hour];
    if (h->temp == UNKNOWN && hour + 1 < 24) h = &day.hours[hour + 1];
    drawHourSlot(col, hour, h->temp == UNKNOWN && h->sky == UNKNOWN ? nullptr : h);
  }
}

void displayWeather(const WeatherData& w, const char* title, int nowHour, const char* footer,
                    const char* warning) {
  drawHeader(title);

  char key[sizeof(bodyCache)];
  snprintf(key, sizeof(key), "%s|%d|%d|%d|%d|%d|%d", w.day.date, (int)w.tomorrow,
           w.now.ok ? w.now.temp10 : 99999, (int)w.day.tmin10, (int)w.day.tmax10, nowHour,
           (int)w.ok);
  if (changed(bodyCache, sizeof(bodyCache), key)) {
    for (auto& row : rowCache) row[0] = '\0';  // 버스 화면 기억은 지움
    drawWeatherBody(w, nowHour);
  }
  drawFooter(footer, warning, "터치: 버스");
}
