"""Writes the made-up news site for the console test: rss.xml, atom.xml, page.html (it links
to rss.xml), article.html (a whole article page) and two pictures. Serve this folder with:
  docker run -d --name kaireader-feeds-test -p 8099:8000 -v "<this folder>:/srv:ro" -w /srv python:3-alpine python -m http.server 8000
"""
import pathlib
import struct
import sys
import zlib

OUT = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else '.')


def png(path, w, h, top, bottom):
    rows = b''
    for y in range(h):
        t = y / (h - 1)
        c = bytes(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        rows += b'\0' + c * w
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF)
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b'')
    (OUT / path).write_bytes(data)


STORY = ('<p>The new ferry left the harbor at dawn, carrying forty passengers, two bicycles and a crate of '
         'apples for the island market.</p><p>Its captain said the crossing took less than an hour, even with '
         'the wind against them, and that from next week it sails twice a day.</p>')

RSS = '''<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:content="http://purl.org/rss/1.0/modules/content/" xmlns:dc="http://purl.org/dc/elements/1.1/">
<channel>
<title>The Harbor Gazette (test)</title>
<link>/page.html</link>
<description>Made-up news for testing KaiReader.</description>
<item><title>New ferry makes its first crossing</title><link>article.html</link><guid>harbor-1</guid>
<dc:creator>Ana Example</dc:creator><pubDate>Thu, 26 Nov 2026 08:00:00 +0100</pubDate>
<enclosure url="pictures/ferry.png" type="image/png" length="1000"/>
<description><![CDATA[<p>Only the start of the story: open <b>Read the full article</b> in the + menu.</p>]]></description></item>
<item><title>Fog &amp;amp; wind expected tonight</title><link>article.html#fog</link><guid>harbor-2</guid>
<pubDate>Wed, 25 Nov 2026 18:30:00 GMT</pubDate>
<content:encoded><![CDATA[<p>Fog rolls in after dark, with wind from the west.</p><p><img src="pictures/fog.png" alt="Fog over the pier"> Ferries may run late.</p>]]></content:encoded></item>
<item><title>Lighthouse opens to visitors</title><link>article.html#lighthouse</link><guid>harbor-3</guid>
<pubDate>Tue, 24 Nov 2026 10:00:00 GMT</pubDate>
<description>Every afternoon from 2 to 5, &lt;i&gt;free&lt;/i&gt;.</description></item>
</channel></rss>
'''

ATOM = '''<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
<title>Lighthouse Notes (test)</title>
<link href="/page.html"/>
<updated>2026-11-26T09:00:00Z</updated>
<entry><title>Keeping the lamp lit</title><id>tag:notes.test,2026:1</id>
<link rel="alternate" href="article.html#lamp"/><published>2026-11-26T09:00:00Z</published>
<author><name>Mara Fennick</name></author>
<content type="xhtml"><div xmlns="http://www.w3.org/1999/xhtml"><p>Oil <em>every</em> night, and wipe the glass.</p></div></content></entry>
<entry><title>Notes on the spring tides</title><id>tag:notes.test,2026:2</id>
<link href="article.html#tides"/><updated>2026-11-23T12:00:00Z</updated>
<summary>High water at 6:10 and 18:32.</summary></entry>
</feed>
'''

PAGE = '''<!DOCTYPE html><html><head><meta charset="utf-8"><title>The Harbor Gazette (test)</title>
<link rel="alternate" type="application/rss+xml" title="RSS" href="rss.xml"></head>
<body><h1>The Harbor Gazette (test)</h1><p>A made-up site: its feed is found from this page.</p></body></html>
'''

ARTICLE = '''<!DOCTYPE html><html><head><meta charset="utf-8"><title>New ferry</title>
<script>var tracking = 1;</script><style>body{font-family:serif}</style></head><body>
<nav><a href="/">Home</a> | <a href="/news">News</a> | <a href="/sport">Sport</a></nav>
<article><h1>New ferry makes its first crossing</h1>''' + STORY + '''
<p><img src="pictures/ferry.png" alt="The ferry at dawn"> The harbor master watched from the pier.</p>
<p>Tickets cost the same as the old boat's, and children under twelve travel free.</p></article>
<aside><p>Related: the fish market moves</p></aside><footer><p>Copyright (made up)</p></footer>
</body></html>
'''

OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'pictures').mkdir(exist_ok=True)
for name, text in (('rss.xml', RSS), ('atom.xml', ATOM), ('page.html', PAGE), ('article.html', ARTICLE)):
    (OUT / name).write_text(text, encoding='utf-8', newline='\n')
png('pictures/ferry.png', 160, 240, (40, 90, 160), (230, 200, 150))
png('pictures/fog.png', 160, 240, (120, 130, 140), (220, 225, 230))
print('written to', OUT)
