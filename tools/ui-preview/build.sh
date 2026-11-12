#!/bin/sh
# Build and run the UI preview on a PC, then convert the images to PNG.
# Needs: g++, Python with Pillow, and one "pio run" so the libraries are downloaded.

set -e
cd "$(dirname "$0")"

ROOT=../..
GFX="$ROOT/.pio/libdeps/seeed_xiao_esp32s3/Adafruit GFX Library"
JSON="$ROOT/.pio/libdeps/seeed_xiao_esp32s3/ArduinoJson/src"

mkdir -p out
# ArduinoJson without the Arduino String, Stream and Print support (the stub has no real ones)
g++ -std=c++14 -O1 -DARDUINO=100 \
    -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 \
    -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0 \
    -Istub -I"$GFX" -I"$JSON" -I"$ROOT/src/ui" \
    preview.cpp "$GFX/Adafruit_GFX.cpp" \
    "$ROOT/src/ui/Components.cpp" \
    "$ROOT/src/ui/WeatherIcons.cpp" \
    "$ROOT/src/app/Pomodoro.cpp" \
    "$ROOT/src/services/IcsParser.cpp" \
    "$ROOT/src/services/GitHubParser.cpp" \
    "$ROOT/src/services/NowPlaying.cpp" \
    "$ROOT/src/app/WidgetLayout.cpp" \
    "$ROOT"/src/ui/pages/*.cpp \
    -o out/preview

./out/preview
python -c "
import glob
from PIL import Image
for path in glob.glob('out/*.pbm'):
    Image.open(path).save(path[:-4] + '.png')
print('Saved PNG files in out/')
"
