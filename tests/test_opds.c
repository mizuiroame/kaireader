// opds.c: OPDS feeds as Calibre-Web, Komga and Kavita write them (made up here).
#include <string.h>

#include "check.h"
#include "opds.h"

#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

// A navigation feed (Calibre-Web's start): its subsections, a search link (not a feed to follow).
static const char NAV[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<feed xmlns=\"http://www.w3.org/2005/Atom\" xmlns:opds=\"http://opds-spec.org/2010/catalog\">\n"
    "  <title>Calibre-Web</title>\n"
    "  <link rel=\"search\" href=\"/opds/osd\" type=\"application/opensearchdescription+xml\"/>\n"
    "  <entry><title>Alphabetical Books</title><id>/opds/books</id>\n"
    "    <link rel=\"subsection\" href=\"/opds/books\" type=\"application/atom+xml;profile=opds-catalog\"/></entry>\n"
    "  <entry><title>Authors</title><id>/opds/author</id>\n"
    "    <link href=\"author\" type=\"application/atom+xml;profile=opds-catalog;kind=navigation\"/></entry>\n"
    "  <entry><title>Again</title><id>x</id>\n"
    "    <link rel=\"subsection\" href=\"/opds/books\" type=\"application/atom+xml;profile=opds-catalog\"/></entry>\n"
    "</feed>\n";

// An acquisition feed: books with several formats, covers, a next page, and entries to skip.
static const char BOOKS[] =
    "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
    "  <title>Books</title>\n"
    "  <link rel=\"next\" href=\"/opds/books?offset=30\" type=\"application/atom+xml\"/>\n"
    "  <entry>\n"
    "    <title>The Quiet   Lighthouse</title><id>urn:uuid:1</id>\n"
    "    <author><name>Ana Example</name></author>\n"
    "    <category term=\"sea\" label=\"Sea Stories\"/>\n"
    "    <link rel=\"http://opds-spec.org/image\" href=\"/opds/cover/1\" type=\"image/jpeg\"/>\n"
    "    <link rel=\"http://opds-spec.org/image/thumbnail\" href=\"/opds/thumb/1\" type=\"image/jpeg\"/>\n"
    "    <link rel=\"http://opds-spec.org/acquisition\" href=\"/opds/download/1/pdf/\" type=\"application/pdf\" length=\"900\"/>\n"
    "    <link rel=\"http://opds-spec.org/acquisition\" href=\"/opds/download/1/epub/\" type=\"application/epub+zip\" length=\"512300\"/>\n"
    "  </entry>\n"
    "  <entry>\n"
    "    <title>Tide &amp; Tables</title><id>urn:uuid:2</id>\n"
    "    <link rel=\"http://opds-spec.org/acquisition/open-access\" href=\"https://cdn.example/b/2.cbz\" type=\"application/zip\"/>\n"
    "    <link rel=\"http://opds-spec.org/image\" href=\"/c/2.png\"/>\n"
    "  </entry>\n"
    "  <entry><title>Buy me</title><id>urn:uuid:3</id>\n"
    "    <link rel=\"http://opds-spec.org/acquisition/buy\" href=\"/buy/3\" type=\"application/epub+zip\"/></entry>\n"
    "  <entry><title>Audio</title><id>urn:uuid:4</id>\n"
    "    <link rel=\"http://opds-spec.org/acquisition\" href=\"/a/4.mp3\" type=\"audio/mpeg\"/></entry>\n"
    "  <entry><title>By name only</title><id>urn:uuid:5</id>\n"
    "    <link rel=\"http://opds-spec.org/acquisition\" href=\"/opds/download/5/cbr\"/></entry>\n"
    "</feed>\n";

static void testResolve(void)
{
    char out[OPDS_URL_MAX];
    CHECK(opdsResolve("http://nas:8083/opds", "/opds/books", out, sizeof(out)));
    CHECK_STR(out, "http://nas:8083/opds/books");
    CHECK(opdsResolve("http://nas:8083/opds/", "author", out, sizeof(out)));
    CHECK_STR(out, "http://nas:8083/opds/author");
    CHECK(opdsResolve("http://nas:8083/opds/books?offset=30", "x/y", out, sizeof(out)));
    CHECK_STR(out, "http://nas:8083/opds/x/y");
    CHECK(opdsResolve("https://k.local/api/opds/key", "https://cdn.example/a.epub", out, sizeof(out)));
    CHECK_STR(out, "https://cdn.example/a.epub");
    CHECK(opdsResolve("https://k.local", "a", out, sizeof(out)));
    CHECK_STR(out, "https://k.local/a");
    CHECK(opdsResolve("https://k.local/x", "//cdn.example/a", out, sizeof(out)));
    CHECK_STR(out, "https://cdn.example/a");
    CHECK(!opdsResolve("not a url", "a", out, sizeof(out)));
    CHECK(!opdsResolve("http://a/", "", out, sizeof(out)));
    CHECK(!opdsResolve("http://a/", "/long", out, 10));
}

static void testKinds(void)
{
    CHECK_STR(opdsExtensionFor("application/epub+zip", NULL), ".epub");
    CHECK_STR(opdsExtensionFor("application/pdf; charset=binary", NULL), ".pdf");
    CHECK_STR(opdsExtensionFor("application/zip", NULL), ".cbz");
    CHECK_STR(opdsExtensionFor(NULL, "/download/5/cbr"), ".cbr");
    CHECK_STR(opdsExtensionFor("application/octet-stream", "/books/Tale.FB2"), ".fb2");
    CHECK(opdsExtensionFor("audio/mpeg", "/a.mp3") == NULL);
}

static void testFeeds(void)
{
    OpdsFeed f;
    CHECK(opdsParse(NAV, sizeof(NAV) - 1, "http://nas:8083/opds/", &f));
    CHECK_STR(f.title, "Calibre-Web");
    CHECK(f.book_count == 0 && f.feed_count == 2 && !f.next[0]);  // The same feed twice: once.
    if (f.feed_count == 2) {
        CHECK_STR(f.feeds[0].url, "http://nas:8083/opds/books");
        CHECK_STR(f.feeds[0].title, "Alphabetical Books");
        CHECK_STR(f.feeds[1].url, "http://nas:8083/opds/author");
    }
    opdsFeedFree(&f);

    CHECK(opdsParse(BOOKS, sizeof(BOOKS) - 1, "http://nas:8083/opds/books", &f));
    CHECK_STR(f.next, "http://nas:8083/opds/books?offset=30");
    CHECK(f.book_count == 3 && f.feed_count == 0);  // Not the one to buy, nor the audio.
    if (f.book_count == 3) {
        OpdsBook *a = &f.books[0], *b = &f.books[1], *c = &f.books[2];
        CHECK_STR(a->title, "The Quiet Lighthouse");
        CHECK_STR(a->author, "Ana Example");
        CHECK_STR(a->genre, "Sea Stories");
        CHECK_STR(a->id, "urn:uuid:1");
        CHECK_STR(a->acquire, "http://nas:8083/opds/download/1/epub/");  // The EPUB, not the PDF.
        CHECK_STR(a->ext, ".epub");
        CHECK(a->size == 512300);
        CHECK_STR(a->cover, "http://nas:8083/opds/thumb/1");  // The thumbnail.
        CHECK_STR(b->title, "Tide & Tables");
        CHECK_STR(b->acquire, "https://cdn.example/b/2.cbz");
        CHECK_STR(b->ext, ".cbz");
        CHECK_STR(b->cover, "http://nas:8083/c/2.png");
        CHECK(b->size == -1 && !b->author[0]);
        CHECK_STR(c->ext, ".cbr");
        CHECK(!c->cover[0]);
    }
    opdsFeedFree(&f);

    CHECK(!opdsParse("<html><body>Login</body></html>", 31, "http://a/", &f));
    CHECK(!opdsParse("not xml at all <", 16, "http://a/", &f) || f.book_count == 0);
    opdsFeedFree(&f);
}

void testOpds(void)
{
    testResolve();
    testKinds();
    testFeeds();
}
