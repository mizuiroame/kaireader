# Installing

[Back to the README](../README.md)

You need a Switch 2 with **kaiHEN** running (launch a game while holding
**R** to open hbmenu), and a way to copy files to the card (OunceShell's FTP
server, for example).

1. Copy `KaiReader.nro` to `sdmc:/switch/kaireader/`.
2. Put your books in `sdmc:/books/` (folders inside it are fine).
3. Open KaiReader from hbmenu.

## Names with accents

The console hides files and folders whose names have accents, ñ, curly quotes
or Japanese, Chinese or Korean in them: they are on the card, but no app can
list them. Copy your books with
[KaiDrop](https://github.com/mizuiroame/kaidrop), which gives them plain names
on the console and leaves your files on the PC as they are, or rename them
first. KaiReader shows each book's real title, from inside the book.

## Where KaiReader keeps its files

KaiReader keeps its files in its config folder, next to the NRO
(`sdmc:/switch/kaireader/config/`): `shelf.txt` and `shelf-network.txt`
(where you left each book, and your favorites), `settings.txt`,
`bookmarks.txt`, `network.txt` and `server.txt` (the accounts: see
[Security](security.md)), `shelf-server.txt`, `server-covers.txt`, `covers/`,
and the `lang` and `Backgrounds` folders.
Books opened from the network folder are copied to `sdmc:/books/.network/`, and
from the book server to `sdmc:/books/.opds/`; free books go to `sdmc:/books/Free/`.

## Updating

Once a day KaiReader looks on GitHub for a new version and says so. From
`sdmc:/switch` it installs it (Settings > About); installed from Brewstore,
Brewstore updates it.
