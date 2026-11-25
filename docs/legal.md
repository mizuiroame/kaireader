# Legal notice

KaiReader is an independent project. It is not affiliated with, endorsed by
or sponsored by Nintendo, Amazon, Artifex Software, Project Gutenberg,
Standard Ebooks, textos.info, Ebooks libres et gratuits or any publisher. "Kindle" is a trademark of Amazon; KaiReader does not use it.

## Your books

KaiReader opens the books you put on your SD card. It does not come with
books, and does not point to any site that offers books without the
author's or publisher's permission.

**Free books** lists only catalogs that publish public-domain books for
free download through a public feed made for reading apps, and downloads a
book only when you choose it, one at a time:

| Catalog | Feed | Books |
|---------|------|-------|
| [Project Gutenberg](https://www.gutenberg.org/) | OPDS | Public domain in the US; check your country's laws |
| [Standard Ebooks](https://standardebooks.org/) | Its Atom feed of new releases, open to all | Public domain in the US; its own work CC0 |
| [textos.info](https://www.textos.info/) | OPDS | Public-domain texts, in Spanish |
| [Ebooks libres et gratuits](https://www.ebooksgratuits.com/) | OPDS | Public-domain texts in its volunteers' editions, in French, free for personal use; at most 50 downloads a day, one at a time |

Public domain depends on the country: a book free in one may not be in
another. Check your own country's rules.

**DRM.** KaiReader does not open books locked with DRM (Kindle, Adobe, Apple,
Kobo, Readium LCP...) and will not remove DRM: in the US (DMCA §1201), the EU
(Directive 2001/29/EC, art. 6) and many other places, getting around it is
against the law even for a book you bought. Many stores and publishers sell
books without DRM; those, and the public domain, open fine.

**Font obfuscation.** Some EPUB files "obfuscate" the fonts they bring, as
the EPUB standard (IDPF font obfuscation) describes, so the font can't be
taken out of the book and installed. Undoing it to show the book is part of
the standard every reader follows; it is not DRM.

## Licenses

KaiReader is free software under the **GNU Affero General Public License,
version 3 or later** ([LICENSE](../LICENSE)), because it is built on MuPDF,
which is under that license. You can share it and change it; if you share a
changed version (or run it for others over a network), you must share its
source code under the same license. Its source code is at
<https://github.com/mizuiroame/kaireader>.

| Part | Use | License | Text |
|------|-----|---------|------|
| [MuPDF](https://mupdf.com/) 1.28.6, Artifex Software | Opens and draws the books | AGPL-3.0-or-later | [third_party/MuPDF-AGPL-3.0.txt](../third_party/MuPDF-AGPL-3.0.txt) |
| FreeType (in MuPDF) | Fonts | FreeType License | [third_party/FreeType-FTL.txt](../third_party/FreeType-FTL.txt) |
| HarfBuzz (in MuPDF) | Text shaping | MIT ("Old MIT") | [third_party/HarfBuzz-MIT.txt](../third_party/HarfBuzz-MIT.txt) |
| libjpeg (in MuPDF) | JPEG images | IJG | [third_party/libjpeg-IJG-README.txt](../third_party/libjpeg-IJG-README.txt) |
| OpenJPEG (in MuPDF) | JPEG 2000 images | BSD-2-Clause | [third_party/OpenJPEG-BSD-2.txt](../third_party/OpenJPEG-BSD-2.txt) |
| jbig2dec (in MuPDF) | JBIG2 images in PDFs | AGPL-3.0-or-later | [third_party/jbig2dec-LICENSE.txt](../third_party/jbig2dec-LICENSE.txt) |
| lcms2mt (in MuPDF) | Color management | MIT | [third_party/lcms2-MIT.txt](../third_party/lcms2-MIT.txt) |
| Gumbo (in MuPDF) | HTML parsing (EPUB, FB2, MOBI) | Apache-2.0 | [third_party/gumbo-Apache-2.0.txt](../third_party/gumbo-Apache-2.0.txt) |
| zlib (in MuPDF) | Compressed data | zlib | [third_party/zlib-LICENSE.txt](../third_party/zlib-LICENSE.txt) |
| brotli (in MuPDF) | WOFF2 fonts | MIT | [third_party/brotli-MIT.txt](../third_party/brotli-MIT.txt) |
| [libarchive](https://libarchive.org/) | Comics in RAR, 7z and tar (CBR, CB7, CBT) | BSD-2-Clause | [third_party/libarchive-LICENSE.txt](../third_party/libarchive-LICENSE.txt) |
| liblzma (XZ Utils), with libarchive | Compressed archives | 0BSD | [third_party/liblzma-0BSD.txt](../third_party/liblzma-0BSD.txt) |
| LZ4, with libarchive | Compressed archives | BSD-2-Clause | [third_party/lz4-BSD-2.txt](../third_party/lz4-BSD-2.txt) |
| Zstandard, with libarchive | Compressed archives | BSD-3-Clause | [third_party/zstd-BSD.txt](../third_party/zstd-BSD.txt) |
| bzip2, with libarchive | Compressed archives | bzip2 license | [third_party/bzip2-LICENSE.txt](../third_party/bzip2-LICENSE.txt) |
| Expat, with libarchive | XML in archives | MIT | [third_party/expat-MIT.txt](../third_party/expat-MIT.txt) |
| [libsmb2](https://github.com/sahlberg/libsmb2) | Network folders over SMB | LGPL-2.1 | [third_party/libsmb2-LGPL-2.1.txt](../third_party/libsmb2-LGPL-2.1.txt) ([its notice](../third_party/libsmb2-COPYING.txt)) |
| [curl](https://curl.se/) | Network folders over WebDAV and FTP | curl | [third_party/curl-LICENSE.txt](../third_party/curl-LICENSE.txt) |
| [libssh2](https://libssh2.org/) | Network folders over SFTP | BSD-3-Clause | [third_party/libssh2-BSD-3-Clause.txt](../third_party/libssh2-BSD-3-Clause.txt) |
| [Jansson](https://github.com/akheron/jansson) | Reading GitHub's answer when looking for an update | MIT | [third_party/Jansson-MIT.txt](../third_party/Jansson-MIT.txt) |
| [Mbed TLS](https://www.trustedfirmware.org/projects/mbed-tls/) | Encryption (SFTP, https) | Apache-2.0 | [third_party/MbedTLS-Apache-2.0.txt](../third_party/MbedTLS-Apache-2.0.txt) |
| Two root certificates (SSL.com) from [Mozilla's CA list](https://wiki.mozilla.org/CA), as [curl publishes it](https://curl.se/docs/caextract.html) | Project Gutenberg's secure connection, whose roots the console lacks (`data/roots_sslcom.pem`) | MPL-2.0 | <https://www.mozilla.org/MPL/2.0/> |
| [Noto Sans](https://fonts.google.com/noto) | The app's own text | SIL OFL 1.1 | [third_party/NotoSans-OFL.txt](../third_party/NotoSans-OFL.txt) |
| [stb_image, stb_truetype](https://github.com/nothings/stb) | Images and the app's fonts | Public domain or MIT | In the files |
| [libnx](https://github.com/switchbrew/libnx) and [libounce](https://github.com/ouncebrew/libounce) | Switch 2 system access | ISC | [third_party/libnx-ISC.txt](../third_party/libnx-ISC.txt), [third_party/libounce-ISC.txt](../third_party/libounce-ISC.txt) |
| [Mesa](https://mesa3d.org/) (OunceVK, via libounce) | Drawing with the GPU | MIT | [third_party/Mesa-MIT.txt](../third_party/Mesa-MIT.txt) |

This software is based in part on the work of the Independent JPEG Group.
Portions of this software are copyright © 2026 The FreeType Project
(www.freetype.org). All rights reserved.

Japanese, Chinese and Korean text is drawn with the console's own system
fonts, read from the console while KaiReader runs; they are not copied into
KaiReader.

**libsmb2 (LGPL 2.1).** It is built into KaiReader unchanged, from its
release 6.0.0 ([smb/Dockerfile](../smb/Dockerfile) builds it). As KaiReader's
whole source code is public, you can build KaiReader again with a changed
libsmb2, as the LGPL allows.

KaiReader's code comes in part from KaiPlayer, KaiRadio and KaiVideo (MIT,
same author), and is under the AGPL here as part of KaiReader.
