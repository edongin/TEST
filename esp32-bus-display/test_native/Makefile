# PC에서 API 응답 해석 테스트 실행:  make
# ArduinoJson 이 없으면 .deps 폴더에 자동으로 받습니다.
# 이미 받아 둔 게 있으면:  make ARDUINOJSON_DIR=/경로/ArduinoJson

ARDUINOJSON_DIR ?= .deps/ArduinoJson
SKETCH_DIR := ../BusArrivalDisplay
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
            -I$(SKETCH_DIR) -I$(ARDUINOJSON_DIR)/src

.PHONY: test clean
test: test_parse test_weather
	./test_parse
	./test_weather

SOURCES := $(SKETCH_DIR)/bus_api.cpp $(SKETCH_DIR)/arrival_text.cpp
WEATHER_SOURCES := $(SKETCH_DIR)/weather_api.cpp $(SKETCH_DIR)/app_logic.cpp $(SKETCH_DIR)/bus_api.cpp

test_parse: test_parse.cpp $(SOURCES) $(wildcard $(SKETCH_DIR)/*.h) | $(ARDUINOJSON_DIR)
	$(CXX) $(CXXFLAGS) test_parse.cpp $(SOURCES) -o $@

test_weather: test_weather.cpp $(WEATHER_SOURCES) $(wildcard $(SKETCH_DIR)/*.h) | $(ARDUINOJSON_DIR)
	$(CXX) $(CXXFLAGS) test_weather.cpp $(WEATHER_SOURCES) -o $@

.deps/ArduinoJson:
	git clone --depth 1 -b v7.4.2 https://github.com/bblanchon/ArduinoJson.git $@

clean:
	rm -f test_parse test_weather
