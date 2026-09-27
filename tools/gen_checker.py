"""Write assets/checker.png: 256x256 RGB, 8x8 orange/white checker.

Used by RenderLab's texture check (adjacent cells must differ on screen).
Stdlib only. Regenerate with: python3 tools/gen_checker.py
"""
import os
import struct
import sys
import zlib

SIZE = 256
CELLS = 8
A = (230, 120, 40)
B = (240, 240, 240)


def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "assets", "checker.png")
    cell = SIZE // CELLS
    rows = bytearray()
    for y in range(SIZE):
        rows.append(0)
        for x in range(SIZE):
            rows.extend(A if ((x // cell) + (y // cell)) % 2 == 0 else B)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 9))
    png += chunk(b"IEND", b"")
    with open(out, "wb") as f:
        f.write(png)
    print(f"wrote {len(png)}B -> {os.path.normpath(out)}")


if __name__ == "__main__":
    main()
