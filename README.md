# KaiReader

A **book reader** for **Switch 2** running
[kaiHEN](https://github.com/ouncebrew/kaihen), a sibling of
[KaiPlayer](https://github.com/mizuiroame/kaiplayer),
[KaiRadio](https://github.com/mizuiroame/kairadio) and
[KaiVideo](https://github.com/mizuiroame/kaivideo). Your DRM-free books from
the SD card, as on an e-reader: **EPUB**, **PDF**, comics (**CBZ**), **FB2**,
**MOBI**, **TXT** and **XPS**, drawn with [MuPDF](https://mupdf.com/).

> **Version 0.1: a first test.** Only books on the SD card for now; network
> folders, book servers (OPDS, Calibre-Web, Kavita, Komga) and public-domain
> catalogs come next.

> KaiReader is an independent project. It is not affiliated with, endorsed by
> or sponsored by Nintendo or Amazon. See the [legal notice](docs/legal.md).

## What it can do

- **Library**: every book in `sdmc:/books` (and its folders), with its cover,
  title, author and how far you've read; the ones you're reading come first.
- **Reading**: books that flow (EPUB, FB2, MOBI, TXT) are laid out for the
  screen, with **ten text sizes** and four **colors** (light, sepia, dark and
  black). PDFs and comics show their own pages, fitted to the screen.
- Picks up **where you left off**, even after changing the text size.
- **Contents**: the book's table of contents, to jump to a chapter; a slider
  to jump anywhere.
- Japanese, Chinese and Korean books use the console's own fonts.
- **Touch and buttons**: tap or swipe to turn the page.

## Controls

| | Library | Reading |
|---|---|---|
| **A** | Read the book selected | Next page |
| **Right**, **R**, **ZR** / **Left**, **L**, **ZL** | Move | Next / previous page |
| **Up** / **Down** | Move | Bigger / smaller text |
| **X** | | Contents |
| **Y** | Look for books again | Colors |
| **−** | | Show or hide the bars |
| **B** | | Back to the library |
| **+** | Menu | Menu |

On the touch screen: tap the right or left side of the page to turn it, or
swipe; tap the middle for the bars.

## Install

You need a Switch 2 with **kaiHEN** running (launch a game while holding
**R** to open hbmenu), and a way to copy files to the card (OunceShell's FTP
server, for example).

1. Copy `KaiReader.nro` to `sdmc:/switch/kaireader/`.
2. Put your books in `sdmc:/books/` (folders inside it are fine).
3. Open KaiReader from hbmenu.

KaiReader keeps its files next to the NRO: `shelf.txt` (where you left each
book), `settings.txt` and `covers/`.

## Build

With Docker:

```
docker build -t kaireader-devkit mupdf
docker build -t kaireader-host -f tests/Dockerfile.host tests
.\build.ps1            # KaiReader.nro
.\build.ps1 test       # host tests with sanitizers
.\build.ps1 preview    # the screens, drawn on the PC
```

The `kaireader-devkit` image starts from KaiRadio's image (the Switch 2
devkit with libounce). The tests open made-up books written by
`tests/make_fixtures.py`.

## License

GNU AGPL 3.0 or later ([LICENSE](LICENSE)), as MuPDF is. The other parts and
their licenses are in the [legal notice](docs/legal.md).
