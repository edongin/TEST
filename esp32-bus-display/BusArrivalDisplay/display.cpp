#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Arduino.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>

#include "app_config.h"
#include "arrival_text.h"

// ----- 화면 배치 (가로 320 x 세로 240) -----
static const int16_t W = 320;
static const int16_t H = 240;
static const int16_t HEADER_H = 30;
static const int16_t FOOTER_H = 20;
static const int16_t ROW_H = (H - HEADER_H - FOOTER_H) / ROWS_PER_PAGE;  // 38
static const int16_t ROUTE_W = 96;  // 왼쪽 노선 번호 칸 너비
static const int16_t TEXT_X = ROUTE_W + 6;
static const int16_t TEXT_W = W - TEXT_X - 6;

// 한글이 들어가는 글꼴 (16픽셀, 자주 쓰는 한글 2,350자 포함)
#define FONT_KOREAN u8g2_font_unifont_t_korean2
// 노선 번호용 큰 영문/숫자 글꼴
#define FONT_ROUTE_BIG u8g2_font_helvB18_tf
#define FONT_ROUTE_MID u8g2_font_helvB14_tf

// 화면이 쓰는 SPI 통로. 다른 통로는 나중에 터치용으로 남겨 둡니다.
#if TFT_USE_HSPI && defined(HSPI)
static SPIClass tftSpi(HSPI);
#elif defined(VSPI)
static SPIClass tftSpi(VSPI);
#else
static SPIClass tftSpi(FSPI);
#endif
static Adafruit_ILI9341 tft(&tftSpi, TFT_DC, TFT_CS, TFT_RST);
static U8G2_FOR_ADAFRUIT_GFX u8f;

static uint16_t C_BG, C_ROW_ALT, C_HEADER, C_TEXT, C_DIM, C_ROUTE, C_SOON, C_WARN;

// 마지막으로 그린 내용을 기억해서, 바뀐 곳만 다시 그립니다.
static char headerCache[96];
static char rowCache[ROWS_PER_PAGE][200];
static char footerCache[160];

static void clearCaches() {
  headerCache[0] = '\0';
  footerCache[0] = '\0';
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
  tftSpi.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(TFT_SPI_HZ);
  tft.setRotation(TFT_ROTATION);
  tft.invertDisplay(TFT_INVERT_COLORS);

  u8f.begin(tft);
  u8f.setFontMode(1);  // 글자 배경은 투명 (배경은 미리 칠해 둠)
  u8f.setFontDirection(0);

  C_BG = ILI9341_BLACK;
  C_ROW_ALT = tft.color565(18, 22, 30);
  C_HEADER = tft.color565(20, 60, 140);
  C_TEXT = ILI9341_WHITE;
  C_DIM = tft.color565(150, 150, 150);
  C_ROUTE = tft.color565(90, 210, 255);
  C_SOON = tft.color565(255, 150, 0);
  C_WARN = tft.color565(255, 80, 80);

  tft.fillScreen(C_BG);
  clearCaches();
}

void displaySetSleep(bool sleep) {
  if (sleep) {
    tft.fillScreen(C_BG);
    // 백라이트를 끌 수 없으면 화면만 검게 칠해 둡니다.
    // (백라이트가 켜진 채 DISPOFF 를 보내면 화면이 하얗게 보임)
    if (TFT_BL >= 0) {
      tft.sendCommand(ILI9341_DISPOFF);
      digitalWrite(TFT_BL, LOW);
    }
  } else {
    if (TFT_BL >= 0) {
      tft.sendCommand(ILI9341_DISPON);
      digitalWrite(TFT_BL, HIGH);
    }
    tft.fillScreen(C_BG);
  }
  clearCaches();
}

void displayMessage(const char* title, const char* line1, const char* line2) {
  tft.fillScreen(C_BG);
  clearCaches();
  u8f.setFont(FONT_KOREAN);
  tft.fillRect(0, 0, W, HEADER_H, C_HEADER);
  drawClipped(8, 21, W - 16, title, C_TEXT);
  if (line1) drawCentered(H / 2 - 14, line1, C_TEXT);
  if (line2) drawCentered(H / 2 + 14, line2, C_DIM);
}

// ---------------------------------------------------------------------------

static void drawHeader(const char* station) {
  char clock[8] = "";
  struct tm now;
  if (getLocalTime(&now, 0)) strftime(clock, sizeof(clock), "%H:%M", &now);

  char key[sizeof(headerCache)];
  snprintf(key, sizeof(key), "%s|%s", station, clock);
  if (!changed(headerCache, sizeof(headerCache), key)) return;

  tft.fillRect(0, 0, W, HEADER_H, C_HEADER);
  u8f.setFont(FONT_KOREAN);
  int16_t clockW = clock[0] ? u8f.getUTF8Width(clock) : 0;
  drawClipped(8, 21, W - 24 - clockW, station, C_TEXT);
  if (clock[0]) drawText(W - 8 - clockW, 21, clock, C_TEXT);
}

static void drawRouteName(int16_t y0, const char* name) {
  // 영문/숫자로만 된 노선 번호는 큰 글꼴, 한글이 있으면 (예: "종로09") 한글 글꼴
  if (isAscii(name)) {
    u8f.setFont(FONT_ROUTE_BIG);
    if (u8f.getUTF8Width(name) > ROUTE_W - 8) u8f.setFont(FONT_ROUTE_MID);
  } else {
    u8f.setFont(FONT_KOREAN);
  }
  int16_t baseline = y0 + (ROW_H + u8f.getFontAscent()) / 2;
  drawClipped(8, baseline, ROUTE_W - 8, name, C_ROUTE);
}

static void drawRow(int index, const bus::Route* route, uint32_t ageSec) {
  char first[64] = "", second[64] = "";
  bus::Urgency urgency = bus::URGENCY_NONE;
  if (route) {
    urgency = bus::formatFirst(route->next[0], ageSec, first, sizeof(first));
    bus::formatSecond(route->next[1], ageSec, second, sizeof(second));
  }

  // 두 번째 줄: "다음 15분 / 범계역 방면" (글꼴에 없는 기호는 피하고 ASCII 만 씀)
  char line2[128] = "";
  if (route) {
    if (second[0] && route->dest[0]) {
      snprintf(line2, sizeof(line2), "%s / %s", second, route->dest);
    } else {
      snprintf(line2, sizeof(line2), "%s", second[0] ? second : route->dest);
    }
  }

  char key[sizeof(rowCache[0])];
  snprintf(key, sizeof(key), "%s|%s|%s|%d", route ? route->name : "", first, line2, urgency);
  if (!changed(rowCache[index], sizeof(rowCache[index]), key)) return;

  int16_t y0 = HEADER_H + index * ROW_H;
  tft.fillRect(0, y0, W, ROW_H, (index % 2) ? C_ROW_ALT : C_BG);
  if (!route) return;

  drawRouteName(y0, route->name);

  uint16_t color = urgency == bus::URGENCY_SOON   ? C_SOON
                   : urgency == bus::URGENCY_NONE ? C_DIM
                                                  : C_TEXT;
  u8f.setFont(FONT_KOREAN);
  drawClipped(TEXT_X, y0 + 17, TEXT_W, first, color);
  drawClipped(TEXT_X, y0 + 34, TEXT_W, line2, C_DIM);
}

static void drawEmpty() {
  const char* key = "<empty>";
  if (!changed(rowCache[0], sizeof(rowCache[0]), key)) return;
  for (int i = 1; i < ROWS_PER_PAGE; i++) rowCache[i][0] = '\0';
  tft.fillRect(0, HEADER_H, W, H - HEADER_H - FOOTER_H, C_BG);
  u8f.setFont(FONT_KOREAN);
  drawCentered(HEADER_H + (H - HEADER_H - FOOTER_H) / 2, "지금 도착 예정인 버스가 없어요", C_DIM);
}

static void drawFooter(const char* footer, const char* warning, int page, int pages) {
  char pageText[12] = "";
  if (pages > 1) snprintf(pageText, sizeof(pageText), "%d/%d", page + 1, pages);
  bool warn = warning && warning[0];

  char key[sizeof(footerCache)];
  snprintf(key, sizeof(key), "%s|%s|%s", warn ? warning : footer, pageText, warn ? "!" : "");
  if (!changed(footerCache, sizeof(footerCache), key)) return;

  int16_t y0 = H - FOOTER_H;
  tft.fillRect(0, y0, W, FOOTER_H, C_BG);
  tft.drawFastHLine(0, y0, W, C_ROW_ALT);
  u8f.setFont(FONT_KOREAN);
  int16_t pageW = pageText[0] ? u8f.getUTF8Width(pageText) : 0;
  drawClipped(8, H - 4, W - 24 - pageW, warn ? warning : footer, warn ? C_WARN : C_DIM);
  if (pageText[0]) drawText(W - 8 - pageW, H - 4, pageText, C_DIM);
}

void displayArrivals(const bus::Result& data, uint32_t ageSec, const char* station,
                     const char* footer, const char* warning, int page) {
  int pages = (data.count + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
  if (pages < 1) pages = 1;
  page %= pages;

  drawHeader(station);
  if (data.count == 0) {
    drawEmpty();
  } else {
    if (strcmp(rowCache[0], "<empty>") == 0) rowCache[0][0] = '\0';
    for (int i = 0; i < ROWS_PER_PAGE; i++) {
      int idx = page * ROWS_PER_PAGE + i;
      drawRow(i, idx < data.count ? &data.routes[idx] : nullptr, ageSec);
    }
  }
  drawFooter(footer, warning, page, pages);
}
