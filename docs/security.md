# Security

[Back to the README](../README.md)

Under kaiHEN, homebrew runs with broad system access. A bug in an app that
reads untrusted files could therefore affect the whole console. This page says
what KaiReader does to keep that risk small, and where it can't.

## The clearest risk: an old libcurl

Every web request (WebDAV, FTP, the book server, Free books, the update check)
goes through **libcurl 7.69.1**, the version devkitPro's Switch packages
provide. It is from 2020, and later curl releases fixed known security
problems (CVEs) in it. KaiReader limits what curl is asked to do (below), but
it can't fix curl itself: this stays open until devkitPro ships a newer curl.
Until then, prefer servers on your own network, and `https://`.

## Passwords on the card

The accounts are in KaiReader's config folder on the SD card
(`sdmc:/switch/kaireader/config/`), in plain files, **not encrypted**. Anyone
who can read the card (or take it out and put it in a PC) can read them.

- **Network folder, shared folder (SMB)**: `network.txt` holds the password's
  **NT hash**, not the password as typed. The password can't be read back
  from it, but for SMB **the hash is as good as the password**: it is what
  Windows signs in with, so whoever has the file can sign in to that share as
  you.
- **Network folder, WebDAV, FTP or SFTP**: the password is only **encoded**
  (base64) in `network.txt`, since these send the password itself. Encoded is
  not encrypted: it reads back in one step.
- **Book server**: `server.txt` keeps its password encoded the same way.
- **"Remember the password" off** (Settings > Network folder): nothing of the
  password is written to the card; KaiReader asks for it when the folder is
  first used, and forgets it when it closes. This is the only setting that
  keeps the network folder's password off the card.
- A new address forgets the old user and password, so they're never sent to
  another server. **Forget** in Settings takes the address, user and password
  off the card.

Give KaiReader a user of its own that can only read the books, on the network
folder and on the book server.

## The network

- **Nothing on the network unless asked.** Besides the update check, KaiReader
  only talks to the network folder and the book server you set up, and to
  the free catalogs (Project Gutenberg, Standard Ebooks, textos.info,
  Ebooks libres et gratuits and unglue.it, and the sites unglue.it's books are
  kept on) while Free books is open. Nothing is
  asked of the free catalogs until Free books is opened.
- **Only http and https** for web requests (no `file:` or other schemes, even
  through a redirect), and an `https://` address can't be redirected to plain
  `http://`. FTP uses only `ftp://` and `ftps://`.
- **Certificates are checked** (https, and FTP over TLS), except where you turn
  **Certificate check** off, for a server with a certificate it made itself:
  the book server's own address (Settings > Book server), or a WebDAV network
  folder over https (Settings > Network folder). Each switch covers only its
  own server; the free catalogs and GitHub are always checked. With it off,
  anyone who can get between the console and that server could pretend to be
  it and receive the password, so use it only on your own network. A new
  address turns the network folder's check back on.
- **Shared folders are signed.** SMB connections require signing, so nobody on
  the network can change what's read; **Allow without signing** turns that off
  for an old NAS.
- **SFTP servers are remembered** by their key (SHA-256): a different key later
  is refused, as it may be another server pretending to be it.
- **The book server's sign-in** only goes to the server's own address, never to
  a cover or book elsewhere, nor to a free catalog.
- **What servers send is bounded.** A catalog page is read up to 8 MB and a
  cover up to 4 MB; feeds are parsed with length-checked buffers, and a web page
  in a feed's place is an error, not an empty list.

## Updates

Installing an update downloads `KaiReader.nro` only from this repository's
GitHub release, over `https://` with the certificate checked, checks its size
and SHA-256 against what GitHub lists for it, and only then puts it in place.

## Books

Books are parsed by MuPDF and the libraries it uses (and libarchive for
comics). A malformed or hostile book is the main remaining risk: only open
books from sources you trust. KaiReader is built with
`-fstack-protector-strong` and `-fwrapv`, and the host tests run under
AddressSanitizer and UndefinedBehaviorSanitizer.

KaiReader opens no book with DRM and has no DRM support of any kind.
