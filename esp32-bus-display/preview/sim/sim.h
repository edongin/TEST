// 미리보기 프로그램이 가짜 하드웨어를 조종할 때 쓰는 함수들
#pragma once

#include <stdint.h>

// 화면 시계에 보일 한국 시간을 정합니다.
void simSetTime(int year, int month, int day, int hour, int minute, int second = 0);
void simClearTime();  // "시계 아직 안 맞음" 상태
void simSetMillis(uint32_t ms);

// 지금 화면을 PNG 로 저장합니다. scale = 확대 배율 (2 면 640x480).
bool simSavePng(const char* path, int scale);

// 여러 장을 한 장에 모은 그림 (cols 칸씩)
void simCapture(const char* name);
bool simSaveSheet(const char* path, int cols, int scale);
