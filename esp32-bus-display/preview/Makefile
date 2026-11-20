# PC 에서 화면 미리보기 만들기:  make
#   결과: out/*.png (화면별), out/all.png (모두 한 장에)
# 필요한 라이브러리(Adafruit GFX, U8g2 글꼴, ArduinoJson)는 .deps 폴더에 자동으로 받습니다.

SKETCH_DIR := ../BusArrivalDisplay
DEPS := .deps
GFX_DIR := $(DEPS)/Adafruit-GFX-Library
U8G2_DIR := $(DEPS)/U8g2_for_Adafruit_GFX
JSON_DIR := $(DEPS)/ArduinoJson

# 보드는 Pico W 로 흉내 냅니다 (config.example.h 의 Pico 핀 설정을 씀)
DEFINES := -DARDUINO=10819 -DARDUINO_ARCH_RP2040 \
           -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 \
           -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0
CXXFLAGS ?= -O1 -g
CXXFLAGS += -std=c++17 $(DEFINES) -Isim -I$(SKETCH_DIR) -I$(GFX_DIR) -I$(U8G2_DIR)/src -I$(JSON_DIR)/src
CFLAGS ?= -O1
CFLAGS += -I$(U8G2_DIR)/src

APP_SOURCES := $(SKETCH_DIR)/display.cpp $(SKETCH_DIR)/arrival_text.cpp \
               $(SKETCH_DIR)/bus_api.cpp $(SKETCH_DIR)/weather_api.cpp
LIB_SOURCES := $(GFX_DIR)/Adafruit_GFX.cpp $(U8G2_DIR)/src/U8g2_for_Adafruit_GFX.cpp
SIM_SOURCES := preview.cpp sim/sim_hardware.cpp sim/sim_png.cpp

.PHONY: run clean
run: preview
	./preview

# 글꼴 파일은 크고 바뀌지 않아서 따로 한 번만 컴파일합니다.
$(DEPS)/u8g2_fonts.o: | $(U8G2_DIR)
	$(CC) $(CFLAGS) -c $(U8G2_DIR)/src/u8g2_fonts.c -o $@

preview: $(SIM_SOURCES) $(APP_SOURCES) $(wildcard sim/*.h) $(wildcard $(SKETCH_DIR)/*.h) \
         $(DEPS)/u8g2_fonts.o | $(GFX_DIR) $(U8G2_DIR) $(JSON_DIR)
	$(CXX) $(CXXFLAGS) $(SIM_SOURCES) $(APP_SOURCES) $(LIB_SOURCES) $(DEPS)/u8g2_fonts.o -o $@

$(GFX_DIR):
	git -c advice.detachedHead=false clone -q --depth 1 -b 1.12.1 https://github.com/adafruit/Adafruit-GFX-Library.git $@

$(U8G2_DIR):
	git -c advice.detachedHead=false clone -q --depth 1 -b 1.8.0 https://github.com/olikraus/U8g2_for_Adafruit_GFX.git $@

$(JSON_DIR):
	git -c advice.detachedHead=false clone -q --depth 1 -b v7.4.2 https://github.com/bblanchon/ArduinoJson.git $@

clean:
	rm -rf preview preview.exe out $(DEPS)/u8g2_fonts.o
