"""Draws KaiReader's icon (icon.jpg, 256x256): an open book, its lines of text cut out.

Usage: python tools/make_icon.py [OUT]
"""
import sys

from icon_render import Canvas, capsule, polygon, save_icon, union

AMBER = (242, 181, 74)
CORNER = 6.0


def draw() -> Canvas:
    canvas = Canvas()
    # Two pages, a little apart at the spine, their outer corners a little higher.
    left = polygon([(38, 78), (122, 92), (122, 190), (38, 176)]) - CORNER
    right = polygon([(134, 92), (218, 78), (218, 176), (134, 190)]) - CORNER
    book = union(left, right)
    canvas.long_shadow(book)
    canvas.fill(book, AMBER)
    # Lines of text, following each page's slant.
    for i in range(5):
        y = 106 + i * 15
        width = 58 if i < 4 else 34
        canvas.cut(capsule((52, y - 6), (52 + width, y + 3), 2.6))
        canvas.cut(capsule((204 - width, y + 3), (204, y - 6), 2.6))
    return canvas


def main() -> None:
    save_icon(draw().result(), sys.argv[1] if len(sys.argv) > 1 else "icon.jpg")


if __name__ == "__main__":
    main()
