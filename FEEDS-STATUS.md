# News (RSS/Atom feeds) for KaiReader: status

Branch `feat/feeds`, worktree `D:\ns2homebrew\kaireader-feeds`, from main at 2b006c8 (1.2.0).
Work paused at the owner's request after the research phase. **No code has been written yet.**
No Docker containers were started.

## Done

Read through the code the feature will hook into, and made the design below.

- Sources: `app.h` `LIB_CARD/NETWORK/SERVER/FREE`, `LIB_SOURCES 4`; `SHELF_FILES[]` in `app.c`;
  `appSourceSet`, `nextSource`, `appLibTab` (stash per source), `sortShelf` (Free keeps the feed's
  own order), `appRescan`, `appOpenBook`, `appMenuRows`/`menuPick`/`appMenuGroup`, the
  L/R/Y handling for Free around `app.c:1406`, touch at `app.c:1610` and `1717`.
- UI: `ui.c:330-395` (header subtitle, "Search" pill for Free), `ui.c:572-585` (menu lines),
  `ui_net.c:25-35` (source names and colors), `ui_net.c:78-155` (loading / problem / empty texts).
- Free books pattern to copy: `app_free.c` (shelf shown vs. stash, `ask`/`takePage`,
  `appFreeEntered`, `free_problem`), job in `netjob.c` (`netJobCatalogPage`/`netJobTakeCatalogPage`).
- Settings: `settings.h/.c` (pages, `SETTINGS_ROWS_MAX 8`, picked-value rows, colors per source
  `SETTING_NET_COLOR/SRV_COLOR`, certificate rows), `theme.c` `themeApplySources`.
- Parser to reuse: `opds.c` uses MuPDF's `fz_parse_xml` with namespace-stripping `tagIs`/`child`/
  `textInto`, and `opdsResolve` for relative links, `opdsBusyStatus` for 502/503/504.
- MuPDF 1.28.6 facts (checked in its `source/fitz/xml.c`):
  - `fz_parse_xml` converts UTF-16 (BOM), and `encoding=` iso-8859-1/-7, koi8-r, windows-1250/1251/1252
    to UTF-8 itself; also `<meta http-equiv content-type charset=>`. Other charsets: not converted.
  - It handles CDATA and the XML entities + numeric ones only; HTML named entities (`&nbsp;`, `&eacute;`)
    in titles need our own small decoder.
  - `fz_parse_xml_from_html5` (gumbo) is built in (HTML engine on): use it for article HTML
    (content:encoded, page for "Read the full article", feed discovery `<link rel=alternate>`),
    and as a fallback for feeds that aren't well-formed XML.
- `doc.c` already opens `.xhtml`/`.html` as reflowable (same text size, colors, two pages, upright).
- `net.c`: curl, follows redirects (max 5, https can't go to http), no `CURLOPT_ACCEPT_ENCODING`,
  no effective-URL output. Not yet checked: whether the Switch portlibs' libcurl has zlib
  (`docker run --rm kaireader-devkit sh -c 'curl-config --features'` or check `libcurl.pc`).
- No fuzz target exists in the repo (none to add to, unless the owner wants one created).

## Design (proposed, not yet built)

- `LIB_NEWS 4`, `LIB_SOURCES 5`, name N_("News"), its own color (`COLOR_NEWS`, + `SETTING_NEWS_COLOR`
  with "Its own"/accents like the others). Always "set" only when at least one feed exists; otherwise
  X skips it and Settings > News says how to add one (as the network folder does).
- Pure modules, host-tested: `feeds.c/.h` (parse RSS 2.0, Atom 1.0, RSS 1.0/RDF into items: id/guid,
  title, link, date as time_t (RFC 822, RFC 3339, bad dates -> 0, sorted last), author, summary,
  content HTML, image from enclosure/media:content/media:thumbnail/first <img>; entity decode;
  relative links via `opdsResolve`; `feedsDiscover(html, base)` for `<link rel=alternate>`;
  OPML import), `feedlist.c/.h` (the feed list file `feeds.txt` in the config folder: url, name,
  check-certificate flag; read/unread ids `news-read.txt`), `article.c/.h` (wrap item HTML as XHTML
  for MuPDF: sanitize to a tag whitelist, drop scripts/styles/iframes; readability: `<article>`,
  then `<main>`, then the element with the most paragraph text).
- Net: `NetRequest` gains `compressed` (Accept-Encoding) and an effective-URL out buffer, used only by
  feed requests. A netjob "feeds refresh" fetches each feed in turn (per-feed certificate flag via
  `any_certificate`), writes each raw answer to `config/news/<hash>.xml` as the offline cache, and
  hands parsed items back. Busy servers: plain "isn't answering" text via `opdsBusyStatus`.
- Library: articles are shelf entries `news:<feed hash>/<item hash>`, title = article title,
  author = feed name, genre = date text; order newest first (own order like Free); unread = never
  opened (percent -1), shown with the existing "on card"-style mark slot; covers from the item image
  via `opdsRememberCover`/`opdsFetchCover`. L/R: All feeds / each feed (like Free's lists).
  + menu: "Mark all as read", "Show <next feed>", "Load the list again" (existing MENU_RESCAN, refresh),
  view, source, Settings, Exit. Y: refresh (to decide; Free uses Y for search).
- Reading: the article written to `config/news/articles/<hash>.xhtml` and opened with `readingOpen`;
  images: first version = placeholders (alt text / "[picture]") unless cheap enough to download up to
  N images next to the .xhtml in the same job. Reader + menu "Read the full article": fetch the link,
  extract main text, rewrite the .xhtml, reopen at the start. Best-effort, said so in its notice.
- Settings > News page (<= 8 rows): Add a feed (keyboard, accepts a page address -> discovery),
  Import an OPML file (sdmc:/switch/kaireader/config/feeds.opml, or any .opml in the books folder),
  Feed (stepped among feeds), Rename this feed, Remove this feed, Certificate check (for that feed),
  Color.
- No bundled newspaper presets.

## Left, in order

1. Check libcurl zlib in the devkit image; add `compressed` + effective URL to `net.c`.
2. `feeds.c` + `tests/test_feeds.c` with fixtures (RSS 2.0, Atom, RDF, broken XML, entities, CDATA,
   bad/various dates, relative links, latin-1) added to tests/Makefile `TESTED`/`TESTS`.
3. `feedlist.c` + tests; `article.c` (XHTML wrap, sanitize, readability) + tests.
4. netjob refresh job + offline cache; `app_news.c` (copy of app_free.c's shape); wire LIB_NEWS
   through app.c, ui.c, ui_net.c, app_welcome.c (decide if the welcome gets a row), settings.c/h,
   theme.c, prefs (color, last feed shown).
5. Strings in all 9 languages (`data/lang_*.txt`), `python tools/lang_check.py` must pass.
6. Preview screens (tests/preview: add the new .c files to `UI_SRC`, made-up feeds in net_host.c),
   all languages, 0 touch problems; button + touch for every control.
7. Docs: `docs/features.md` News section, `docs/controls.md`, limitations; screenshots from the
   preview, signed with `switch2homebrew-submit/tools/watermark.py --repos`, made-up feeds only.
8. Live test with the local server (below), then `.\build.ps1 nro`, and
   `python <scratchpad>\patch_title.py KaiReader.nro KaiReader-news-test.nro "KaiReader News test"`
   (the patch script was in the session scratchpad
   `C:\Users\sebas\AppData\Local\Temp\claude\f7d124a0f073f3bb\a5d7cb97-7243-4e6d-a1b2-7607e8802fbf\scratchpad\patch_title.py`;
   if it's gone, ask the coordinator for it). Don't upload to the console.
9. Spanish console test steps verified against `data/lang_es.txt`.

## Open questions for the owner

- Y in News: refresh, or "Mark all as read"? (Free uses Y for search; the network folder doesn't use Y.)
- Should News appear in the first-run welcome as a fifth choice?
- Pictures: download up to a few per article (slower to open, uses card space) or placeholders only in v1?
- Keep how many articles per feed / how long? Proposal: the last 100 per feed, 30 days of read marks.
- A fuzz target: the repo has none; create a first one (libFuzzer on the host) for the feed parser, or skip?

## How to resume / test server

Build and tests: `.\build.ps1 nro|test|preview` (PowerShell), see `docs/building.md`.
Commit as `git -c user.name=Mizuiro -c user.email=257194107+mizuiroame@users.noreply.github.com commit`,
conventional messages ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
Don't push, merge, tag or release.

Local feed server (made-up content only), from a folder with `rss.xml`, `atom.xml`, `page.html`
(a page with `<link rel="alternate" type="application/rss+xml" href="rss.xml">`) and `article.html`:

```
docker run -d --name kaireader-feeds-test -p 8099:8000 -v "<folder>:/srv:ro" -w /srv python:3-alpine python -m http.server 8000
```

On the console type `http://192.168.0.168:8099/rss.xml` (or `192.168.0.168:8099/page.html` to test
discovery). Afterwards: `docker rm -f kaireader-feeds-test`.
