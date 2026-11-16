"""Convert the preview's PBM images (P4, black and white) to PNG.

Only the Python standard library, no Pillow needed.

Usage: python3 pbm2png.py out/*.pbm   (each image is saved next to it as .png)
"""

import struct
import sys
import zlib


def read_pbm(path):
    with open(path, "rb") as file:
        data = file.read()

    # Header: "P4", width, height, separated by whitespace (# starts a comment),
    # then one whitespace byte and the bitmap
    tokens = []
    pos = 0
    while len(tokens) < 3:
        while data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            pos = data.index(b"\n", pos)
            continue
        start = pos
        while not data[pos:pos + 1].isspace():
            pos += 1
        tokens.append(data[start:pos])
    if tokens[0] != b"P4":
        raise ValueError("{} is not a binary PBM (P4) file".format(path))
    width, height = int(tokens[1]), int(tokens[2])
    pos += 1

    row_bytes = (width + 7) // 8
    bitmap = data[pos:pos + row_bytes * height]
    if len(bitmap) != row_bytes * height:
        raise ValueError("{} is cut short".format(path))
    return width, height, row_bytes, bitmap


def write_png(path, width, height, row_bytes, bitmap):
    # PBM: 1 = black. A 1-bit greyscale PNG: 0 = black, so every bit is flipped.
    flipped = bytes(byte ^ 0xFF for byte in bitmap)
    raw = b"".join(b"\x00" + flipped[row * row_bytes:(row + 1) * row_bytes] for row in range(height))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    header = struct.pack(">IIBBBBB", width, height, 1, 0, 0, 0, 0)  # 1 bit, greyscale
    with open(path, "wb") as file:
        file.write(b"\x89PNG\r\n\x1a\n")
        file.write(chunk(b"IHDR", header))
        file.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        file.write(chunk(b"IEND", b""))


def main(paths):
    for path in paths:
        width, height, row_bytes, bitmap = read_pbm(path)
        write_png(path[:-4] + ".png", width, height, row_bytes, bitmap)
    print("Saved {} PNG files in out/".format(len(paths)))


if __name__ == "__main__":
    main(sys.argv[1:])
