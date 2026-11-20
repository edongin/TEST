// 화면 그림을 PNG 파일로 저장합니다. 외부 라이브러리 없이 (압축 없는 PNG).
#include <Adafruit_ILI9341.h>

#include <string>
#include <vector>

#include "sim.h"

namespace {

uint32_t crcTable[256];
void initCrc() {
  for (uint32_t n = 0; n < 256; n++) {
    uint32_t c = n;
    for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crcTable[n] = c;
  }
}
uint32_t crc(const uint8_t* p, size_t n, uint32_t c = 0xFFFFFFFFu) {
  for (size_t i = 0; i < n; i++) c = crcTable[(c ^ p[i]) & 0xFF] ^ (c >> 8);
  return c;
}

void put32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(x >> 24);
  v.push_back(x >> 16);
  v.push_back(x >> 8);
  v.push_back(x);
}

void chunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> buf;
  put32(buf, (uint32_t)data.size());
  buf.insert(buf.end(), type, type + 4);
  buf.insert(buf.end(), data.begin(), data.end());
  uint32_t c = crc(buf.data() + 4, buf.size() - 4) ^ 0xFFFFFFFFu;
  put32(buf, c);
  fwrite(buf.data(), 1, buf.size(), f);
}

// RGB 그림(w x h x 3바이트)을 PNG 로 저장
bool writePng(const char* path, int w, int h, const std::vector<uint8_t>& rgb) {
  static bool crcReady = false;
  if (!crcReady) {
    initCrc();
    crcReady = true;
  }
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  fwrite(sig, 1, 8, f);

  std::vector<uint8_t> ihdr;
  put32(ihdr, w);
  put32(ihdr, h);
  ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});  // 8비트 RGB
  chunk(f, "IHDR", ihdr);

  // 줄마다 필터 0 + RGB
  std::vector<uint8_t> raw;
  raw.reserve((size_t)h * (w * 3 + 1));
  for (int y = 0; y < h; y++) {
    raw.push_back(0);
    raw.insert(raw.end(), rgb.begin() + (size_t)y * w * 3, rgb.begin() + (size_t)(y + 1) * w * 3);
  }
  // zlib (압축 없는 블록)
  std::vector<uint8_t> z = {0x78, 0x01};
  uint32_t a = 1, b = 0;
  for (uint8_t c : raw) {
    a = (a + c) % 65521;
    b = (b + a) % 65521;
  }
  size_t pos = 0;
  do {
    size_t n = raw.size() - pos;
    if (n > 65535) n = 65535;
    bool last = pos + n == raw.size();
    z.push_back(last ? 1 : 0);
    z.push_back(n & 0xFF);
    z.push_back(n >> 8);
    z.push_back(~n & 0xFF);
    z.push_back((~n >> 8) & 0xFF);
    z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
    pos += n;
  } while (pos < raw.size());
  put32(z, (b << 16) | a);
  chunk(f, "IDAT", z);
  chunk(f, "IEND", {});
  return fclose(f) == 0;
}

// RGB565 -> RGB888
void rgbOf(uint16_t c, uint8_t* out) {
  uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
  out[0] = (r << 3) | (r >> 2);
  out[1] = (g << 2) | (g >> 4);
  out[2] = (b << 3) | (b >> 2);
}

struct Shot {
  std::string name;
  int w, h;
  std::vector<uint8_t> rgb;  // 1배 크기
};
std::vector<Shot> shots;

Shot grab() {
  Adafruit_ILI9341* s = simScreen();
  Shot shot;
  shot.w = s->width();
  shot.h = s->height();
  shot.rgb.resize((size_t)shot.w * shot.h * 3);
  for (int y = 0; y < shot.h; y++) {
    for (int x = 0; x < shot.w; x++) {
      uint16_t c = s->sleeping() ? 0 : s->pixel(x, y);
      rgbOf(c, &shot.rgb[((size_t)y * shot.w + x) * 3]);
    }
  }
  return shot;
}

// src(sw x sh) 를 scale 배로 키워 dst(dw 폭) 의 (ox, oy) 에 붙입니다.
void blit(const std::vector<uint8_t>& src, int sw, int sh, int scale, std::vector<uint8_t>& dst,
          int dw, int ox, int oy) {
  for (int y = 0; y < sh * scale; y++) {
    for (int x = 0; x < sw * scale; x++) {
      const uint8_t* p = &src[((size_t)(y / scale) * sw + x / scale) * 3];
      uint8_t* q = &dst[((size_t)(oy + y) * dw + ox + x) * 3];
      q[0] = p[0];
      q[1] = p[1];
      q[2] = p[2];
    }
  }
}

}  // namespace

bool simSavePng(const char* path, int scale) {
  Shot s = grab();
  std::vector<uint8_t> big((size_t)s.w * scale * s.h * scale * 3);
  blit(s.rgb, s.w, s.h, scale, big, s.w * scale, 0, 0);
  return writePng(path, s.w * scale, s.h * scale, big);
}

void simCapture(const char* name) {
  Shot s = grab();
  s.name = name;
  shots.push_back(std::move(s));
}

// 화면들을 회색 틈을 두고 바둑판처럼 붙입니다.
bool simSaveSheet(const char* path, int cols, int scale) {
  if (shots.empty()) return false;
  const int gap = 8 * scale;
  const int sw = shots[0].w * scale, sh = shots[0].h * scale;
  const int n = (int)shots.size();
  if (cols > n) cols = n;
  const int rows = (n + cols - 1) / cols;
  const int W = cols * sw + (cols + 1) * gap, H = rows * sh + (rows + 1) * gap;
  std::vector<uint8_t> sheet((size_t)W * H * 3, 0x55);
  for (int i = 0; i < n; i++) {
    int ox = gap + (i % cols) * (sw + gap), oy = gap + (i / cols) * (sh + gap);
    blit(shots[i].rgb, shots[i].w, shots[i].h, scale, sheet, W, ox, oy);
  }
  return writePng(path, W, H, sheet);
}
