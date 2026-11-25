# KaiReader

<p align="center"><img src="docs/cover.png" alt="KaiReader" width="100%"></p>

A **book reader** for **Switch 2** running
[kaiHEN](https://github.com/ouncebrew/kaihen), a sibling of
[KaiPlayer](https://github.com/mizuiroame/kaiplayer),
[KaiRadio](https://github.com/mizuiroame/kairadio) and
[KaiVideo](https://github.com/mizuiroame/kaivideo). Your DRM-free books from
the SD card, a folder on your network or your book server (Calibre-Web,
Kavita, Komga), and free public-domain books from Project Gutenberg and
Standard Ebooks, as on an e-reader: **EPUB**, **PDF**,
comics (**CBZ**, **CBR**, **CB7**, **CBT**), **FB2**, **MOBI**, **TXT** and
**XPS**, drawn with [MuPDF](https://mupdf.com/).

![Library](docs/screenshots/library.png)

| Reading | Upright, like a book | Sorted by author |
|---------|----------------------|------------------|
| ![A page](docs/screenshots/epub-light.png) | ![Upright](docs/screenshots/upright-epub.png) | ![By author](docs/screenshots/library-author.png) |

| Two pages, like an open book | Sepia, bigger text | Settings |
|------------------------------|--------------------|----------|
| ![Two pages](docs/screenshots/epub-two-pages.png) | ![Sepia](docs/screenshots/epub-sepia-big.png) | ![Settings](docs/screenshots/settings-display.png) |

| A network folder's books | As a list | Favorites |
|--------------------------|-----------|-----------|
| ![Network folder](docs/screenshots/network.png) | ![List](docs/screenshots/library-list.png) | ![Favorites](docs/screenshots/library-favorites-stars.png) |

| Your book server (OPDS) | Free books |
|-------------------------|------------|
| ![Book server](docs/screenshots/server.png) | ![Free books](docs/screenshots/free.png) |

The screenshots show made-up books.

> **Version 0.5, a pre-release: still young.** Only in English for now; the
> other languages come next.

> KaiReader is an independent project. It is not affiliated with, endorsed by
> or sponsored by Nintendo or Amazon. See the [legal notice](docs/legal.md).

## What it can do

- **Library**: every book in `sdmc:/books` (and its folders), with its cover,
  title, author and how far you've read. Sorted by **recently read**, your
  **favorites**, **title**,
  **author** (by surname, when the book gives it) or **genre** (the book's
  subject), with the author or genre above each group.
- **Reading**: books that flow (EPUB, FB2, MOBI, TXT) are laid out for the
  screen, with **ten text sizes** and four **colors** (light, sepia, dark and
  black: the text and the paper change, pictures keep their colors). PDFs and
  comics show their own pages, fitted to the screen.
- **Network folders**: a Windows shared folder (SMB), a NAS or Nextcloud
  (WebDAV), or an FTP or SFTP server (Settings > Network folder). **X** in the
  library switches between its books and the SD card's, both shown the same
  way: KaiReader looks through the folder and its folders, and reads each
  book's cover and title over the network (only the few parts it needs). A book
  you open is copied to the card first, so it opens at once the next time.
- **Your book server**: Calibre-Web, Kavita, Komga or another OPDS catalog
  (Settings > Book server: its address, and your user and password). Its books
  show as the card's, with the server's own covers; one you open is downloaded
  first.
- **Free books**: public-domain books, free to read: Project Gutenberg's most
  popular and its search (**Y**), and Standard Ebooks' newest (**L**/**R**
  change the list). A book you choose is downloaded to `sdmc:/books/Free/` and
  stays in your library.
- **X** goes round your libraries: the SD card, the network folder and the
  book server (the ones set up), and Free books.
- **Favorites**: hold your finger on a cover, or choose "Add to favorites" in
  the + menu; Favorites is one of the sorts (Y).
- **A grid or a list**: covers, or a list with more of each book (the + menu,
  or Settings > Library).
- **Zoom** on PDFs and comics: up and down, or two fingers, up to 3×; the left
  stick or a finger moves the page, drawn sharp at the screen's resolution.
- **Bookmarks**: the left stick pressed in marks the page; the Contents panel
  (X) lists them in its Bookmarks tab, with the first words of each page.
- **Two pages side by side**, like an open book (Settings > Reading > Pages),
  with the console lying down.
- **Upright, like a book**: hold the console with its right or left side down
  (Settings > Display > Screen), or let it turn as you turn the console
  (Automatic). The right stick pressed in switches. The whole app turns, and
  the buttons and the touch screen with it.
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
| **Up** / **Down** | Move | Bigger / smaller text; PDFs and comics: zoom |
| **X** | The next library: SD card, network folder, book server, Free books | Contents and bookmarks |
| **L** / **R** | Free books: the other lists | Previous / next page |
| **Left stick** pressed | | Bookmark the page |
| **Right stick** pressed | Upright or lying down | Upright or lying down |
| **Y** | Sort by: recent, favorites, title, author, genre (Free books: search) | Colors |
| **−** | | Show or hide the bars |
| **B** | | Back to the library |
| **+** | Menu: favorite, SD card or network folder, grid or list, look again, Settings, Exit | Menu: Contents, Bookmark, Library, Settings, Exit |

The right stick moves like the D-pad. Upright, every direction turns with the
console: up is up as you hold it.

On the touch screen: tap the right or left side of the page to turn it, or
swipe; tap the middle for the bars. In the library, hold a cover to make it a
favorite.

## Install

You need a Switch 2 with **kaiHEN** running (launch a game while holding
**R** to open hbmenu), and a way to copy files to the card (OunceShell's FTP
server, for example).

1. Copy `KaiReader.nro` to `sdmc:/switch/kaireader/`.
2. Put your books in `sdmc:/books/` (folders inside it are fine).
3. Open KaiReader from hbmenu.

**Names with accents.** The console hides files and folders whose names have
accents, ñ, curly quotes or Japanese, Chinese or Korean in them: they are on
the card, but no app can list them. Copy your books with
[KaiDrop](https://github.com/mizuiroame/kaidrop), which gives them plain names
on the console and leaves your files on the PC as they are, or rename them
first. KaiReader shows each book's real title, from inside the book.

KaiReader keeps its files next to the NRO: `shelf.txt` and `shelf-network.txt`
(where you left each book, and your favorites), `settings.txt`,
`bookmarks.txt`, `network.txt` and `server.txt` (the accounts), `shelf-server.txt`,
`server-covers.txt` and `covers/`.
Books opened from the network folder are copied to `sdmc:/books/.network/`, and
from the book server to `sdmc:/books/.opds/`; free books go to `sdmc:/books/Free/`.

## Build

With Docker:

```
docker build -t kaireader-devkit mupdf
docker tag kaireader-devkit kaireader-devkit:before-smb
docker build -t kaireader-devkit smb
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
