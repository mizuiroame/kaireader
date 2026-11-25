# What KaiReader can do

[Back to the README](../README.md) · [Screenshots](screenshots.md)

KaiReader reads **EPUB**, **PDF**, comics (**CBZ**, **CBR**, **CB7**,
**CBT**), **FB2**, **MOBI**, **TXT** and **XPS**, drawn with
[MuPDF](https://mupdf.com/). Only DRM-free books open.

## Library

- **Library**: every book in `sdmc:/books` (and its folders), with its cover,
  title, author and how far you've read. Sorted by **recently read**, your
  **favorites**, **title**,
  **author** (by surname, when the book gives it) or **genre** (the book's
  subject), with the author or genre above each group.
- **Favorites**: hold your finger on a cover, or choose "Add to favorites" in
  the + menu; Favorites is one of the sorts (Y).
- **A grid or a list**: covers, or a list with more of each book (the + menu,
  or Settings > Library).
- **A welcome** the first time, when there are no books yet: where yours are,
  or straight to Free books.

## Reading

- **Reading**: books that flow (EPUB, FB2, MOBI, TXT) are laid out for the
  screen, with **ten text sizes** and four **colors** (light, sepia, dark and
  black: the text and the paper change, pictures keep their colors). PDFs and
  comics show their own pages, fitted to the screen.
- **Zoom** on PDFs and comics: up and down, or two fingers, up to 3×; the left
  stick or a finger moves the page, drawn sharp at the screen's resolution.
- **Bookmarks**: the left stick pressed in marks the page; the Contents panel
  (X) lists them in its Bookmarks tab, with the first words of each page.
- **Contents**: the book's table of contents, to jump to a chapter; a slider
  to jump anywhere.
- **Two pages side by side**, like an open book (Settings > Reading > Pages),
  with the console lying down.
- **Upright, like a book**: hold the console with its right or left side down
  (Settings > Display > Screen), or let it turn as you turn the console
  (Automatic). The right stick pressed in switches. The whole app turns, and
  the buttons and the touch screen with it.
- Picks up **where you left off**, even after changing the text size, and
  opens the book you were reading when KaiReader starts (Settings > Reading).
- Japanese, Chinese and Korean books use the console's own fonts.
- **Touch and buttons**: tap or swipe to turn the page. See [Controls](controls.md).

## Where the books come from

**X** goes round your libraries: the SD card, the network folder and the book
server (the ones set up), and Free books. Each wears its color in the
"Source:" pill at the top and in Settings.

- **Network folders**: a Windows shared folder (SMB), a NAS or Nextcloud
  (WebDAV), or an FTP or SFTP server (Settings > Network folder). **X** in the
  library switches between its books and the SD card's, both shown the same
  way: KaiReader looks through the folder and its folders, and reads each
  book's cover and title over the network (only the few parts it needs). A book
  you open is copied to the card first, so it opens at once the next time.
  The folder's page in Settings: Address, User and password, Remember the
  password, Allow without signing (old NAS, SMB only), Certificate check
  (https WebDAV with its own certificate: off), Color, Test the connection and
  Forget this folder.
- **Your book server**: Calibre-Web, Kavita, Komga or another OPDS catalog
  (Settings > Book server: its address, and your user and password). Its books
  show as the card's, with the server's own covers; one you open is downloaded
  first.
- **Free books**: public-domain and freely licensed books, free to read:
  Project Gutenberg's most popular, Standard Ebooks' newest, textos.info's
  most popular (in Spanish), Ebooks libres et gratuits' newest (in French) and
  unglue.it's most popular (books their authors and publishers released under
  Creative Commons licenses, mostly in English); **L**/**R** change the list.
  **Y** searches the catalog of the list you're on, and your last search
  stays one of the lists. The French catalog's feed has no covers: its books
  show their titles until you download them, and its authors (given as
  "Hugo, Victor") show as written, "Victor Hugo". A book you choose is downloaded to `sdmc:/books/Free/` and
  stays in your library. When a catalog is down for maintenance (Project
  Gutenberg was for days in November 2026), KaiReader says so, with **Try
  again**; **L**/**R** still reach the other lists.

How the accounts are kept on the card: [Security](security.md).

## Looks, languages and updates

- **Your colors and a picture**: an accent color, and a photo of yours behind
  the library, as soft as you like (Settings > Display; JPEG or PNG files in
  the `Backgrounds` folder of KaiReader's config folder).
- **In nine languages** (Settings > Display > Language), with more as files in
  the `lang` folder.
- **Updates**: once a day KaiReader looks on GitHub for a new version and says
  so; from `sdmc:/switch` it installs it (Settings > About), from Brewstore,
  Brewstore does.
