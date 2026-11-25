"""A made-up background for the preview: a dusk sky with hills, as a PNG (standard library only)."""
import struct, sys, zlib

W, H = 960, 540
rows = []
for y in range(H):
    row = bytearray([0])
    for x in range(W):
        t = y / H
        r, g, b = int(40 + 180 * t), int(50 + 60 * t), int(120 - 40 * t)
        if y > H * 0.62 + 40 * ((x / W - 0.5) ** 2) * 4:  # The hills.
            r, g, b = 30, 40, 50
        row += bytes((r, g, b))
    rows.append(bytes(row))
raw = zlib.compress(b"".join(rows), 9)


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) + chunk(b"IDAT", raw) + chunk(b"IEND", b"")
open(sys.argv[1], "wb").write(png)
