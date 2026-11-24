"""Makes the made-up books the host tests open (all text invented here).

    python3 make_fixtures.py <folder> [--library]

quiet.epub    EPUB 3: nav table of contents, three chapters, a cover image
old.epub      EPUB 2: NCX table of contents only, cover named by <meta name="cover">
plain.pdf     PDF, three pages (612 x 792 points)
strip.cbz     comic book, three pictures of 300 x 450
tale.fb2      FictionBook 2
notes.txt     plain text
broken.epub   a ZIP with nothing of an EPUB in it
empty.pdf     zero bytes
picture.epub  a page with a red picture, its text black by its own style
library*.epub with --library: eight more, with drawn covers, several authors and genres (previews)
"""

from __future__ import annotations

import os
import struct
import sys
import zipfile
import zlib

WORDS = ("lantern harbor quiet tide gull rope keeper window salt evening north stair "
         "copper bell morning fog island ledger boat wind").split()


def paragraph(seed: int, words: int = 60) -> str:
    return " ".join(WORDS[(seed * 7 + i * 3) % len(WORDS)] for i in range(words)).capitalize() + "."


def png(width: int, height: int, rgb: tuple[int, int, int]) -> bytes:
    row = b"\x00" + bytes(rgb) * width
    raw = row * height

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def art_png(width: int, height: int, sky: tuple, sea: tuple, sun: tuple, seed: int) -> bytes:
    """A made-up cover: a sky fading down, a sun, and a sea with a few wave lines."""
    horizon = int(height * (0.55 + (seed % 3) * 0.05))
    sun_x, sun_y, sun_r = width * (0.3 + (seed % 4) * 0.13), horizon - height * 0.12, width * 0.16
    rows = []
    for y in range(height):
        row = bytearray(b"\x00")
        for x in range(width):
            if y < horizon:
                t = y / horizon
                c = [int(sky[i] * (1 - t) + sun[i] * 0.35 * t + sky[i] * 0.4 * t) for i in range(3)]
                if (x - sun_x) ** 2 + (y - sun_y) ** 2 < sun_r ** 2:
                    c = list(sun)
            else:
                t = (y - horizon) / (height - horizon)
                c = [int(sea[i] * (1 - 0.5 * t)) for i in range(3)]
                wave = (y - horizon) % 22 < 3 and (x + y * 3 + seed * 17) % 60 < 38
                if wave:
                    c = [min(255, v + 45) for v in c]
            row += bytes(min(255, max(0, v)) for v in c)
        rows.append(bytes(row))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))


# Made-up books for the previews' library: title, author, sorted name, genre, cover colors.
LIBRARY = [
    ("The Salt Ledger", "Mara Fennick", "Fennick, Mara", "Mystery", (40, 50, 90), (20, 70, 110), (240, 190, 90)),
    ("North of the Bell", "Ivo Castellan", "Castellan, Ivo", "Adventure", (90, 120, 170), (30, 60, 100), (250, 230, 200)),
    ("Copper Evenings", "Mara Fennick", "Fennick, Mara", "Mystery", (120, 60, 70), (60, 40, 60), (250, 160, 90)),
    ("A Map of Small Islands", "Lio Arden", "Arden, Lio", "Travel", (100, 170, 200), (20, 110, 140), (255, 240, 170)),
    ("The Gull Keeper", "Ivo Castellan", "Castellan, Ivo", "Adventure", (60, 90, 80), (20, 60, 70), (230, 220, 160)),
    ("Lanterns at Low Tide", "Noor Halden", "Halden, Noor", "Poetry", (30, 30, 60), (10, 30, 60), (250, 200, 120)),
    ("Fog Stair", "Lio Arden", "Arden, Lio", "Travel", (150, 150, 160), (70, 80, 90), (240, 240, 230)),
    ("Ropes and Wind", "Noor Halden", "Halden, Noor", "Poetry", (180, 110, 80), (60, 70, 100), (255, 220, 150)),
]


def library_epub(path: str, number: int) -> None:
    title, author, file_as, genre, sky, sea, sun = LIBRARY[number]
    with zipfile.ZipFile(path, "w") as z:
        z.writestr(zipfile.ZipInfo("mimetype"), "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        z.writestr("META-INF/container.xml",
                   '<?xml version="1.0"?><container version="1.0" '
                   'xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>'
                   '<rootfile full-path="content.opf" media-type="application/oebps-package+xml"/>'
                   "</rootfiles></container>")
        z.writestr("cover.png", art_png(200, 300, sky, sea, sun, number))
        z.writestr("ch1.xhtml", chapter_xhtml(title, number * 3 + 1, 25))
        z.writestr("nav.xhtml", '<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml" '
                   'xmlns:epub="http://www.idpf.org/2007/ops"><head><title>Contents</title></head><body>'
                   f'<nav epub:type="toc"><ol><li><a href="ch1.xhtml">{title}</a></li></ol></nav></body></html>')
        z.writestr("content.opf",
                   '<?xml version="1.0" encoding="utf-8"?><package xmlns="http://www.idpf.org/2007/opf" '
                   'version="3.0" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/">'
                   f'<dc:title>{title}</dc:title><dc:creator id="a">{author}</dc:creator>'
                   f'<meta refines="#a" property="file-as">{file_as}</meta><dc:subject>{genre}</dc:subject>'
                   f'<dc:language>en</dc:language><dc:identifier id="uid">urn:uuid:00000000-0000-4000-9000-'
                   f'{number:012d}</dc:identifier></metadata><manifest>'
                   '<item id="cover" href="cover.png" media-type="image/png" properties="cover-image"/>'
                   '<item id="ch1" href="ch1.xhtml" media-type="application/xhtml+xml"/>'
                   '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
                   '</manifest><spine><itemref idref="ch1"/></spine></package>')


def picture_epub(path: str) -> None:
    """A page of text with a red picture in it, and its own text color (black)."""
    with zipfile.ZipFile(path, "w") as z:
        z.writestr(zipfile.ZipInfo("mimetype"), "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        z.writestr("META-INF/container.xml",
                   '<?xml version="1.0"?><container version="1.0" '
                   'xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>'
                   '<rootfile full-path="content.opf" media-type="application/oebps-package+xml"/>'
                   "</rootfiles></container>")
        z.writestr("red.png", png(120, 60, (220, 30, 30)))
        z.writestr("style.css", "p { color: #000000; } body { background-color: #ffffff; }")
        z.writestr("ch1.xhtml", '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE html>\n'
                   '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Picture</title>'
                   '<link rel="stylesheet" type="text/css" href="style.css"/></head><body>'
                   f'<p>{paragraph(9)}</p><p><img src="red.png" alt="red"/></p><p>{paragraph(10)}</p>'
                   '</body></html>')
        z.writestr("content.opf",
                   '<?xml version="1.0" encoding="utf-8"?><package xmlns="http://www.idpf.org/2007/opf" '
                   'version="3.0" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/">'
                   '<dc:title>A Red Picture</dc:title><dc:language>en</dc:language>'
                   '<dc:identifier id="uid">urn:uuid:00000000-0000-4000-8000-000000000009</dc:identifier>'
                   '</metadata><manifest><item id="ch1" href="ch1.xhtml" media-type="application/xhtml+xml"/>'
                   '<item id="red" href="red.png" media-type="image/png"/>'
                   '<item id="css" href="style.css" media-type="text/css"/></manifest>'
                   '<spine><itemref idref="ch1"/></spine></package>')


def chapter_xhtml(title: str, seed: int, paragraphs: int) -> str:
    body = "\n".join(f"<p>{paragraph(seed + i)}</p>" for i in range(paragraphs))
    return ('<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE html>\n'
            '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">'
            f"<head><title>{title}</title></head><body><h1>{title}</h1>\n{body}\n</body></html>")


def epub(path: str, version3: bool) -> None:
    chapters = [("The Keeper", 1, 30), ("Fog at the Stair", 2, 30), ("Morning Bell", 3, 30)]
    with zipfile.ZipFile(path, "w") as z:
        z.writestr(zipfile.ZipInfo("mimetype"), "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        z.writestr("META-INF/container.xml",
                   '<?xml version="1.0"?><container version="1.0" '
                   'xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>'
                   '<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>'
                   "</rootfiles></container>")
        items, spine = [], []
        for i, (title, seed, count) in enumerate(chapters, 1):
            z.writestr(f"OEBPS/ch{i}.xhtml", chapter_xhtml(title, seed, count))
            items.append(f'<item id="ch{i}" href="ch{i}.xhtml" media-type="application/xhtml+xml"/>')
            spine.append(f'<itemref idref="ch{i}"/>')
        points = "".join(f'<navPoint id="n{i}" playOrder="{i}"><navLabel><text>{t}</text></navLabel>'
                         f'<content src="ch{i}.xhtml"/></navPoint>' for i, (t, _, _) in enumerate(chapters, 1))
        z.writestr("OEBPS/toc.ncx",
                   '<?xml version="1.0"?><ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">'
                   f"<navMap>{points}</navMap></ncx>")
        items.append('<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>')
        if not version3:  # EPUB 2 names its cover with <meta name="cover">, in a folder of its own.
            z.writestr("OEBPS/images/front%20page.png".replace("%20", " "), png(80, 120, (30, 140, 60)))
            items.append('<item id="front" href="images/front%20page.png" media-type="image/png"/>')
        if version3:
            z.writestr("OEBPS/cover.png", png(60, 90, (40, 90, 160)))
            items.append('<item id="cover" href="cover.png" media-type="image/png" properties="cover-image"/>')
            links = "".join(f'<li><a href="ch{i}.xhtml">{t}</a></li>' for i, (t, _, _) in enumerate(chapters, 1))
            z.writestr("OEBPS/nav.xhtml",
                       '<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml" '
                       'xmlns:epub="http://www.idpf.org/2007/ops"><head><title>Contents</title></head><body>'
                       f'<nav epub:type="toc"><ol>{links}</ol></nav></body></html>')
            items.append('<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>')
        title = "The Quiet Lighthouse" if version3 else "An Older Harbor"
        # EPUB 3 sorts its author with a refining <meta>; EPUB 2 with opf:file-as.
        creator = ('<dc:creator id="a1">Ana Example</dc:creator><meta refines="#a1" property="file-as">'
                   'Example, Ana</meta><dc:subject>Sea Stories</dc:subject>' if version3 else
                   '<dc:creator opf:file-as="Example, Ana" xmlns:opf="http://www.idpf.org/2007/opf">Ana Example'
                   '</dc:creator><dc:subject>Harbors</dc:subject><dc:subject>Second subject</dc:subject>')
        meta = (f'<dc:title>{title}</dc:title>{creator}'
                '<dc:language>en</dc:language><dc:identifier id="uid">urn:uuid:00000000-0000-4000-8000-'
                f'{"000000000003" if version3 else "000000000002"}</dc:identifier>'
                + ("" if version3 else '<meta name="cover" content="front"/>'))
        z.writestr("OEBPS/content.opf",
                   '<?xml version="1.0" encoding="utf-8"?>'
                   f'<package xmlns="http://www.idpf.org/2007/opf" version="{"3.0" if version3 else "2.0"}" '
                   'unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/">'
                   f'{meta}</metadata><manifest>{"".join(items)}</manifest>'
                   f'<spine toc="ncx">{"".join(spine)}</spine></package>')


def pdf(path: str) -> None:
    objects: list[bytes] = []
    pages = 3
    page_ids = [4 + i * 2 for i in range(pages)]
    objects.append(b"<< /Type /Catalog /Pages 2 0 R >>")
    objects.append(b"<< /Type /Pages /Kids [" + b" ".join(b"%d 0 R" % p for p in page_ids)
                   + b"] /Count %d >>" % pages)
    objects.append(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")
    for i in range(pages):
        text = f"BT /F1 24 Tf 72 700 Td (Made-up page {i + 1}) Tj ET".encode()
        objects.append(b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> "
                       b"/Contents %d 0 R >>" % (page_ids[i] + 1))
        objects.append(b"<< /Length %d >>\nstream\n" % len(text) + text + b"\nendstream")
    out = bytearray(b"%PDF-1.4\n")
    offsets = []
    for n, body in enumerate(objects, 1):
        offsets.append(len(out))
        out += b"%d 0 obj\n" % n + body + b"\nendobj\n"
    xref = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objects) + 1)
    for off in offsets:
        out += b"%010d 00000 n \n" % off
    out += b"trailer\n<< /Size %d /Root 1 0 R /Info << /Title (Made-up Ledger) /Author (Bo Sample) >> >>\n" % (
        len(objects) + 1)
    out += b"startxref\n%d\n%%%%EOF\n" % xref
    with open(path, "wb") as f:
        f.write(out)


def cbz(path: str) -> None:
    colors = [(200, 40, 40), (40, 160, 60), (40, 60, 200)]
    with zipfile.ZipFile(path, "w") as z:
        for i, c in enumerate(colors, 1):
            z.writestr(f"page{i:02d}.png", png(300, 450, c))


def fb2(path: str) -> None:
    sections = "".join(f"<section><title><p>Part {i}</p></title>"
                       + "".join(f"<p>{paragraph(i * 5 + j)}</p>" for j in range(20)) + "</section>"
                       for i in range(1, 3))
    with open(path, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">'
                "<description><title-info><author><first-name>Cy</first-name><last-name>Placeholder</last-name>"
                "</author><book-title>A Tale of Ropes</book-title><lang>en</lang></title-info></description>"
                f"<body>{sections}</body></FictionBook>")


def txt(path: str) -> None:
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n\n".join(paragraph(i) for i in range(40)))


def main() -> None:
    folder = sys.argv[1]
    os.makedirs(folder, exist_ok=True)
    epub(os.path.join(folder, "quiet.epub"), True)
    epub(os.path.join(folder, "old.epub"), False)
    pdf(os.path.join(folder, "plain.pdf"))
    cbz(os.path.join(folder, "strip.cbz"))
    fb2(os.path.join(folder, "tale.fb2"))
    txt(os.path.join(folder, "notes.txt"))
    with zipfile.ZipFile(os.path.join(folder, "broken.epub"), "w") as z:
        z.writestr("hello.txt", "not a book")
    open(os.path.join(folder, "empty.pdf"), "wb").close()
    picture_epub(os.path.join(folder, "picture.epub"))
    if "--library" in sys.argv:  # More books, with drawn covers, for the previews' library.
        for i in range(len(LIBRARY)):
            library_epub(os.path.join(folder, f"library{i + 1}.epub"), i)


if __name__ == "__main__":
    main()
