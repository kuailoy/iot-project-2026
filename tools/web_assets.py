"""Gzip the web pages' style (src/web/style.css) into the firmware.

PlatformIO pre-script. Writes WebAssets.h into the build folder (not into
src/, nothing to commit) with the gzipped bytes and a version: the CRC of
the file, so a changed style gets a new address (/style.css?v=...) and
browsers load it again instead of keeping the old one.
"""

import gzip
import re
import zlib
from pathlib import Path

Import("env")

source = Path(env.subst("$PROJECT_DIR")) / "src" / "web" / "style.css"
out_dir = Path(env.subst("$BUILD_DIR")) / "generated"
header = out_dir / "WebAssets.h"

# Comments are for people only, browsers don't need them
css = re.sub(rb"/\*.*?\*/\s*", b"", source.read_bytes(), flags=re.S)
packed = gzip.compress(css, compresslevel=9, mtime=0)  # mtime=0: same file, same bytes
version = "{:08x}".format(zlib.crc32(css))

lines = [
    "// Made by tools/web_assets.py from src/web/style.css, don't edit",
    "#pragma once",
    "#include <pgmspace.h>",
    "#include <stddef.h>",
    "#include <stdint.h>",
    "",
    'static const char STYLE_CSS_VERSION[] = "{}";'.format(version),
    "static const size_t STYLE_CSS_GZ_LEN = {};".format(len(packed)),
    "static const uint8_t STYLE_CSS_GZ[] PROGMEM = {",
]
for start in range(0, len(packed), 20):
    lines.append("    " + ", ".join(str(b) for b in packed[start:start + 20]) + ",")
lines.append("};")
text = "\n".join(lines) + "\n"

# Only write when it changed, so an unchanged style doesn't rebuild anything
out_dir.mkdir(parents=True, exist_ok=True)
if not header.exists() or header.read_text() != text:
    header.write_text(text)
    print("Web assets: style.css {} -> {} bytes gzipped".format(len(css), len(packed)))

env.Append(CPPPATH=[str(out_dir)])
