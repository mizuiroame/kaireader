# News (RSS/Atom feeds) for KaiReader: status

Branch `feat/feeds`, worktree `D:\ns2homebrew\kaireader-feeds`, from main at 2b006c8 (1.2.0).
**Built** (2026-11-26/27): all steps done; only the console test is left.

## Done

- `net.c`: `NetRequest.compressed` (Accept-Encoding, the Switch's curl has zlib) and `final_url`.
- `feeds.c/.h` (+ `textbuf.c/.h`): RSS 2.0, RSS 1.0/RDF, Atom; dates RFC 822/3339; entities; CDATA;
  xhtml content; pictures (enclosure, media:, itunes:image, first `<img>`); discovery; OPML; merge
  (last 100, newest first); the kept feed written as Atom. `tests/test_feeds.c`.
- `feedlist.c/.h` (feeds.txt, `news:<feed>/<item>` paths, read marks forgotten 30 days after an
  article left the kept 100) and `article.c/.h` (clean XHTML page, "[picture]" placeholders,
  readability for the full article). `tests/test_news.c`.
- `newsjob.c/.h`: check (kept files first, then the sites), find (page -> its feed), full article.
- `app_news.c` + wiring: `LIB_NEWS` (X skips it until a feed is followed), Y refresh, L/R feed,
  + menu "Mark all as read" / "Show <feed>" / "Check the feeds again", reader + menu "Read the full
  article", unread dot ("New" in the list), Settings > News (Add, Import OPML, Feed, Rename,
  Certificate check, Color, Remove), welcome's fifth choice "News from websites", coral color,
  prefs `news_color`/`news_feed`, covers through `opdsRememberCover` ("news:" fetched like "opds:").
- Strings in all 9 languages; `tools/lang_check.py` passes for each.
- Preview: `newsShots` in tests/preview/preview.c (made-up feeds, every language), 0 problems.
- Docs: features.md "News" (with what it can't do yet), controls.md.
- Live: `tests/news-server/make_server.py` (made-up site) and `live.sh`/`live.c` (newsjob against it,
  from the host image): 0 failures.

## Design (as planned; built as above)

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

## Suggested feeds (2026-11-27, owner's request after the console test)

- Settings > News > "Suggested feeds" (second row) opens a page: "Language" (stepped, starts on the
  app's language), then up to 7 feeds of that language; A or a tap follows one exactly as "Add a feed"
  does (same messages and "already followed" check), under the suggestion's name and address; a
  followed one says "Followed". B goes back to News. Table: `source/feedsuggest.c` (9 languages).
- Every suggested URL was fetched live on 2026-11-26 and parsed with feeds.c (one-off host check, nothing
  real committed). `article.c` now leaves out parts beside the article (class/id share, comments,
  related, newsletter, hidden...) when they hold little text.

## Left

- Console test by the owner, with `KaiReader-news-test.nro` ("KaiReader News test", built from this
  branch, in the worktree's root) and the local server `kaireader-feeds-test` (left running). Nothing
  was uploaded. Afterwards: `docker rm -f kaireader-feeds-test`.
- Not done on purpose: pictures inside articles (placeholders only), fuzz target.
- Screenshots (news, news-article, settings-news) are in docs/screenshots, signed per file with
  watermark.py (not `--repos`, which would also touch the other repos' screenshots).

## Original plan (steps 1-9, done unless said above)

1. libcurl zlib, `compressed` + effective URL. 2. `feeds.c` + tests. 3. `feedlist.c`, `article.c` + tests.
4. job, `app_news.c`, wiring. 5. Strings. 6. Preview. 7. Docs, screenshots. 8. Live test, test .nro.
9. Spanish console steps.

## Owner's decisions (2026-11-27)

- Y in News: **refresh** the feeds. "Mark all as read" goes in the + menu.
- First-run welcome: **yes**, News is a fifth choice (it leads to adding the first feed).
- Pictures inside articles: **placeholders only** in v1 (alt text / "[picture]"); the list cover is still fetched.
- Keep the **last 100 articles per feed**, read marks forgotten after **30 days**.
- Fuzz target: skipped (not asked; the parser gets host tests with broken-XML fixtures instead).

## How to resume / test server

Build and tests: `.\build.ps1 nro|test|preview` (PowerShell), see `docs/building.md`.
Commit as `git -c user.name=Mizuiro -c user.email=257194107+mizuiroame@users.noreply.github.com commit`,
conventional messages ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
Don't push, merge, tag or release.

Local feed server (made-up content only): `python tests/news-server/make_server.py tests/news-server/site`
writes `rss.xml`, `atom.xml`, `page.html` (links to rss.xml), `article.html` and two pictures; then

```
docker run -d --name kaireader-feeds-test -p 8099:8000 -v "D:/ns2homebrew/kaireader-feeds/tests/news-server/site:/srv:ro" -w /srv python:3-alpine python -m http.server 8000
```

Host check of newsjob against it: `docker run --rm --security-opt seccomp=unconfined
--add-host=host.docker.internal:host-gateway -v D:/ns2homebrew/kaireader-feeds:/build/kaireader -w
/build/kaireader/tests/news-server kaireader-host sh live.sh`

On the console type `http://192.168.0.168:8099/rss.xml` (or `192.168.0.168:8099/page.html` to test
discovery). Afterwards: `docker rm -f kaireader-feeds-test`.
