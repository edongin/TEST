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
static const int16_t ROW_H = (H - HEADER_H - FOOTER_H) / ROWS_PER_PAGE;  // 38
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

// 글꼴 바꾸기. 이 라이브러리는 글꼴을 바꿀 때마다 "글자 배경 칠하기"로 되돌아가서
// 매번 투명 모드(1)를 다시 켜 줍니다. (안 하면 글자 뒤에 검은 상자가 생김)
static void useFont(const uint8_t* font) {
  u8f.setFont(font);
  u8f.setFontMode(1);
}

static uint16_t C_BG, C_ROW_ALT, C_HEADER, C_TEXT, C_DIM, C_ROUTE, C_SOON, C_WARN;
static uint16_t C_SUN, C_CLOUD, C_DARK_CLOUD, C_RAIN, C_SNOW, C_COLD, C_HOT;

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
  u8f.setFontDirection(0);

  C_BG = ILI9341_BLACK;
  C_ROW_ALT = tft.color565(18, 22, 30);
  C_HEADER = tft.color565(20, 60, 140);
  C_TEXT = ILI9341_WHITE;
  C_DIM = tft.color565(150, 150, 150);
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

void displayMessage(const char* title, const char* line1, const char* line2) {
  tft.fillScreen(C_BG);
  clearCaches();
  useFont(FONT_KOREAN);
  tft.fillRect(0, 0, W, HEADER_H, C_HEADER);
  drawClipped(8, 21, W - 16, title, C_TEXT);
  if (line1) drawCentered(H / 2 - 14, line1, C_TEXT);
  if (line2) drawCentered(H / 2 + 14, line2, C_DIM);
}

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

static void drawRouteName(int16_t y0, const char* name) {
  // 영문/숫자로만 된 노선 번호는 큰 글꼴, 한글이 있으면 (예: "종로09") 한글 글꼴
  if (isAscii(name)) {
    useFont(FONT_ROUTE_BIG);
    if (u8f.getUTF8Width(name) > ROUTE_W - 8) useFont(FONT_ROUTE_MID);
  } else {
    useFont(FONT_KOREAN);
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
  useFont(FONT_KOREAN);
  drawClipped(TEXT_X, y0 + 17, TEXT_W, first, color);
  drawClipped(TEXT_X, y0 + 34, TEXT_W, line2, C_DIM);
}

static void drawEmpty() {
  const char* key = "<empty>";
  if (!changed(rowCache[0], sizeof(rowCache[0]), key)) return;
  for (int i = 1; i < ROWS_PER_PAGE; i++) rowCache[i][0] = '\0';
  tft.fillRect(0, HEADER_H, W, H - HEADER_H - FOOTER_H, C_BG);
  useFont(FONT_KOREAN);
  drawCentered(HEADER_H + (H - HEADER_H - FOOTER_H) / 2, "지금 도착 예정인 버스가 없어요", C_DIM);
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
static int16_t drawBigTemp(int16_t x, int16_t baseline, int temp, uint16_t color) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", temp);
  useFont(FONT_BIG_NUM);
  drawText(x, baseline, buf, color);
  int16_t w = u8f.getUTF8Width(buf);
  int16_t top = baseline - u8f.getFontAscent();
  tft.drawCircle(x + w + 7, top + 5, 4, color);
  tft.drawCircle(x + w + 7, top + 5, 3, color);
  return w + 14;
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
  const int16_t y0 = 152;
  if (!h) return;
  char buf[16];
  useFont(FONT_KOREAN);
  snprintf(buf, sizeof(buf), "%02d시", hour);
  drawCenteredAt(cx, y0 + 15, buf, C_DIM);
  drawWeatherIcon(cx, y0 + 33, 30, weather::hourCondition(*h));
  if (h->temp != weather::UNKNOWN) {
    snprintf(buf, sizeof(buf), "%d도", h->temp);
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  useFont(FONT_KOREAN);
  drawCenteredAt(cx, y0 + 64, buf, C_TEXT);
}

static void drawWeatherBody(const WeatherData& w, int nowHour) {
  using namespace weather;
  const Day& day = w.day;
  tft.fillRect(0, HEADER_H, W, H - HEADER_H - FOOTER_H, C_BG);

  int popAm, popPm;
  Condition am = summarize(day, 6, 12, &popAm);
  Condition pm = summarize(day, 12, 19, &popPm);
  char line[96];

  if (w.tomorrow) {
    // ----- 내일: 대표 날씨 아이콘 + 최저/최고 크게 + 오전/오후 -----
    int dummy;
    drawWeatherIcon(46, 78, 64, summarize(day, 6, 19, &dummy));
    int16_t x = 96;
    if (day.tmin10 != UNKNOWN10 && day.tmax10 != UNKNOWN10) {
      x += drawBigTemp(x, 94, roundTemp(day.tmin10), C_COLD);
      useFont(FONT_BIG_NUM);
      drawText(x, 94, "/", C_DIM);
      x += u8f.getUTF8Width("/") + 8;
      drawBigTemp(x, 94, roundTemp(day.tmax10), C_HOT);
    }
    useFont(FONT_KOREAN);
    snprintf(line, sizeof(line), "최저 / 최고");
    drawText(96, 54, line, C_DIM);
    snprintf(line, sizeof(line), "오전 %s   오후 %s", conditionName(am), conditionName(pm));
    drawClipped(96, 120, W - 104, line, C_TEXT);
  } else {
    // ----- 오늘: 지금 날씨 아이콘 + 지금 기온 크게 + 최저/최고 -----
    int8_t sky = (nowHour >= 0 && nowHour < 24) ? day.hours[nowHour].sky : UNKNOWN;
    Condition current;
    if (w.now.ok) {
      current = nowCondition(w.now, sky);
    } else {
      int dummy;
      current = summarize(day, nowHour, nowHour + 3, &dummy);
    }
    if (current == COND_UNKNOWN) current = am != COND_UNKNOWN ? am : pm;
    drawWeatherIcon(46, 78, 64, current);

    if (w.now.ok) {
      drawBigTemp(96, 94, roundTemp(w.now.temp10), C_TEXT);
    } else if (nowHour >= 0 && nowHour < 24 && day.hours[nowHour].temp != UNKNOWN) {
      drawBigTemp(96, 94, day.hours[nowHour].temp, C_TEXT);
    }
    useFont(FONT_KOREAN);
    drawText(96, 54, "지금", C_DIM);
    if (w.now.ok && w.now.humidity != UNKNOWN) {
      snprintf(line, sizeof(line), "%s   습도 %d%%", conditionName(current), w.now.humidity);
    } else {
      snprintf(line, sizeof(line), "%s", conditionName(current));
    }
    drawClipped(96, 120, 120, line, C_TEXT);

    if (day.tmin10 != UNKNOWN10) {
      snprintf(line, sizeof(line), "최저 %d도", roundTemp(day.tmin10));
      drawText(214, 70, line, C_COLD);
    }
    if (day.tmax10 != UNKNOWN10) {
      snprintf(line, sizeof(line), "최고 %d도", roundTemp(day.tmax10));
      drawText(214, 94, line, C_HOT);
    }
  }

  useFont(FONT_KOREAN);
  popText(line, sizeof(line), popAm, popPm);
  drawClipped(12, 142, W - 24, line, C_DIM);
  tft.drawFastHLine(8, 150, W - 16, C_ROW_ALT);

  // 시간별 예보 5칸 (3시간 간격). 오늘은 다음 시각부터, 내일은 06시부터.
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
