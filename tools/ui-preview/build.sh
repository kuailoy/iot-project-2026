#!/bin/sh
# Build and run the UI preview on a PC, then convert the images to PNG.
# Needs: g++, Python with Pillow, and one "pio run" so the libraries are downloaded.

set -e
cd "$(dirname "$0")"

ROOT=../..
GFX="$ROOT/.pio/libdeps/seeed_xiao_esp32s3/Adafruit GFX Library"

mkdir -p out
g++ -std=c++14 -O1 -DARDUINO=100 \
    -Istub -I"$GFX" -I"$ROOT/src/ui" \
    preview.cpp "$GFX/Adafruit_GFX.cpp" \
    "$ROOT/src/ui/Components.cpp" \
    "$ROOT/src/ui/pages/HomePage.cpp" \
    "$ROOT/src/ui/pages/MenuPage.cpp" \
    -o out/preview

./out/preview
python -c "
from PIL import Image
for name in ('home', 'home-empty', 'menu'):
    Image.open('out/%s.pbm' % name).save('out/%s.png' % name)
print('Saved PNG files in out/')
"
