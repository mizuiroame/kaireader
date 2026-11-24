"""Turns the preview's .ppm screenshots into .png (and removes the .ppm).

    python3 topng.py <folder>
"""
import pathlib
import struct
import sys
import zlib


def to_png(ppm: pathlib.Path) -> None:
    data = ppm.read_bytes()
    parts = data.split(b"\n", 3)
    width, height = map(int, parts[1].split())
    pixels = parts[3]
    rows = b"".join(b"\x00" + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))

    def chunk(kind: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(rows, 6)) + chunk(b"IEND", b""))
    ppm.with_suffix(".png").write_bytes(png)
    ppm.unlink()


for path in sorted(pathlib.Path(sys.argv[1]).glob("*.ppm")):
    to_png(path)
