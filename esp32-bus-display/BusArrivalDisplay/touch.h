// 화면 터치 감지 (XPT2046 터치 칩). 어디를 눌렀는지는 쓰지 않고 "눌렀다"만 압니다.
#pragma once

// displayBegin() 다음에 부르세요. (Pico 는 화면과 SPI 선을 같이 씀)
void touchBegin();

// 손가락을 댄 순간 한 번만 true. 계속 누르고 있어도 다시 true 가 되지 않습니다.
// 터치 대신 쓰는 버튼(Pico BOOTSEL 등)을 눌러도 true.
bool touchTapped();
