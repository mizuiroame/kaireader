// catalogs.c: the free catalogs' lists, and their pages' books (feeds made up, in their formats).
#include <stdlib.h>
#include <string.h>

#include "catalogs.h"
#include "check.h"

#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

// As Project Gutenberg's search pages are: each book an entry leading to its own page.
static const char GUTENBERG_PAGE[] =
    "<feed xmlns=\"http://www.w3.org/2005/Atom\"><title>All Books</title>"
    "<link rel=\"next\" type=\"application/atom+xml;profile=opds-catalog\" "
    "href=\"/ebooks/search.opds/?sort_order=downloads&amp;start_index=26\"/>"
    "<entry><title>Sort Alphabetically by Title</title><id>x</id>"
    "<link type=\"application/atom+xml;profile=opds-catalog\" rel=\"subsection\" href=\"/ebooks/search.opds/?sort_order=title\"/></entry>"
    "<entry><id>https://www.gutenberg.org/ebooks/84.opds</id><title>A Made-up Novel</title>"
    "<content type=\"text\">Ana Example</content>"
    "<link type=\"application/atom+xml;profile=opds-catalog\" rel=\"subsection\" href=\"/ebooks/84.opds\"/>"
    "<link type=\"image/png\" rel=\"http://opds-spec.org/image/thumbnail\" href=\"data:image/png;base64,AAAA\"/></entry>"
    "<entry><id>https://www.gutenberg.org/ebooks/1342.opds</id><title>Caf\xC3\xA9 Stories</title>"
    "<content type=\"text\">Lio Arden</content>"
    "<link type=\"application/atom+xml;profile=opds-catalog\" rel=\"subsection\" href=\"/ebooks/1342.opds\"/></entry>"
    "</feed>";

// As Standard Ebooks' newest: an Atom feed, its downloads as enclosures, its covers as media:thumbnail.
static const char NEWEST_PAGE[] =
    "<feed xmlns=\"http://www.w3.org/2005/Atom\" xmlns:media=\"http://search.yahoo.com/mrss/\"><title>Newest</title>"
    "<entry><id>https://standardebooks.org/ebooks/ana-example/the-quiet-lighthouse</id>"
    "<title>The Quiet Lighthouse</title><author><name>Ana Example</name></author>"
    "<media:thumbnail url=\"https://standardebooks.org/x/cover-thumbnail.jpg\" width=\"350\" height=\"525\"/>"
    "<link href=\"https://standardebooks.org/x\" rel=\"alternate\" type=\"application/xhtml+xml\"/>"
    "<link href=\"https://standardebooks.org/x/a.epub?source=feed\" length=\"875588\" rel=\"enclosure\" type=\"application/epub+zip\"/>"
    "<link href=\"https://standardebooks.org/x/a_advanced.epub?source=feed\" rel=\"enclosure\" type=\"application/epub+zip\"/>"
    "<link href=\"https://standardebooks.org/x/a.kepub.epub?source=feed\" rel=\"enclosure\" type=\"application/kepub+zip\"/>"
    "<link href=\"https://standardebooks.org/x/text/single-page\" rel=\"enclosure\" type=\"application/xhtml+xml\"/>"
    "</entry></feed>";

// Standard Ebooks' newest as it came on 2026-11-24 (trimmed, the book made up): a stylesheet
// before the feed and a script inside it, before its entries.
static const char NEWEST_REAL[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<?xml-stylesheet href=\"https://standardebooks.org/feeds/atom/style\" type=\"text/xsl\"?>\n"
    "<feed xmlns=\"http://www.w3.org/2005/Atom\" xmlns:media=\"http://search.yahoo.com/mrss/\">\n"
    "\t<script xmlns=\"http://www.w3.org/1999/xhtml\" src=\"https://standardebooks.org/scripts/xslt-polyfill.min.js\"/>\n"
    "\t<id>https://standardebooks.org/feeds/atom/new-releases</id>\n"
    "\t<link href=\"https://standardebooks.org/feeds/atom/new-releases\" rel=\"self\" type=\"application/atom+xml\"/>\n"
    "\t<title>Standard Ebooks - Newest Ebooks</title>\n"
    "\t<author>\n\t\t<name>Standard Ebooks</name>\n\t</author>\n"
    "\t<link href=\"https://standardebooks.org/opensearch\" rel=\"search\" title=\"Standard Ebooks\" "
    "type=\"application/opensearchdescription+xml\"/>\n"
    "\t<entry>\n\t\t<id>https://standardebooks.org/ebooks/ana-example/the-salt-ledger</id>\n"
    "\t\t<title>The Salt Ledger</title>\n"
    "\t\t<author>\n\t\t\t<name>Ana Example</name>\n\t\t\t<uri>https://standardebooks.org/ebooks/ana-example</uri>\n\t\t</author>\n"
    "\t\t<rights>Public domain in the United States.</rights>\n"
    "\t\t<category scheme=\"http://purl.org/dc/terms/LCSH\" term=\"Fiction\"/>\n"
    "\t\t<media:thumbnail height=\"525\" url=\"https://standardebooks.org/ebooks/ana-example/the-salt-ledger/downloads/cover-thumbnail.jpg\" width=\"350\"/>\n"
    "\t\t<link href=\"https://standardebooks.org/ebooks/ana-example/the-salt-ledger\" rel=\"alternate\" title=\"This ebook's page at Standard Ebooks\" type=\"application/xhtml+xml\"/>\n"
    "\t\t<link href=\"https://standardebooks.org/ebooks/ana-example/the-salt-ledger/downloads/ana-example_the-salt-ledger.epub?source=feed\" length=\"875588\" rel=\"enclosure\" title=\"Recommended compatible epub\" type=\"application/epub+zip\"/>\n"
    "\t\t<link href=\"https://standardebooks.org/ebooks/ana-example/the-salt-ledger/downloads/ana-example_the-salt-ledger_advanced.epub?source=feed\" length=\"861369\" rel=\"enclosure\" title=\"Advanced epub\" type=\"application/epub+zip\"/>\n"
    "\t\t<link href=\"https://standardebooks.org/ebooks/ana-example/the-salt-ledger/downloads/ana-example_the-salt-ledger.kepub.epub?source=feed\" length=\"802783\" rel=\"enclosure\" title=\"Kobo Kepub epub\" type=\"application/kepub+zip\"/>\n"
    "\t</entry>\n</feed>\n";

// What Project Gutenberg answered for days in November 2026 (HTTP 503, trimmed): a web page, not a feed.
static const char GUTENBERG_DOWN[] =
    "<!DOCTYPE html> <html lang=\"en\"> <head> <meta charset=\"utf-8\"> "
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> "
    "<title>Temporarily Down for Maintenance</title> <style> body { margin: 0; } </style> </head> <body> <main> "
    "<h1>Temporarily Down for Maintenance</h1> "
    "<p>Project Gutenberg is temporarily unavailable while we perform maintenance.</p> "
    "<p>Please check back soon. We apologize for the inconvenience.</p> </main> </body> </html>";

// textos.info's most popular as it came on 2026-11-25 (trimmed, the book made up): OPDS 1, its EPUB
// first, then MOBI and PDF; its next page joined with "&" to the address.
static const char SPANISH_REAL[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<feed xmlns:thr=\"http://purl.org/syndication/thread/1.0\" xmlns:app=\"http://www.w3.org/2007/app\" "
    "xmlns:opds=\"http://opds-spec.org/2010/catalog\" xmlns:dcterms=\"http://purl.org/dc/terms/\" "
    "xmlns=\"http://www.w3.org/2005/Atom\" xml:lang=\"en\">\n"
    "  <id>https://www.textos.info/populares.atom</id>\n"
    "  <title>Textos m\xC3\xA1s populares</title>\n"
    "<link title=\"P\xC3\xA1gina siguiente\" type=\"application/atom+xml;profile=opds-catalog;kind=acquisition\" "
    "href=\"https://www.textos.info/populares.atom&amp;page=2\" rel=\"next\"/>\n"
    "<entry>\n<title>El Faro Callado</title>\n<id>https://www.textos.info/ana-ejemplo/el-faro-callado</id>\n"
    "<author>\n  <name>Ana Ejemplo</name>\n  <uri>https://www.textos.info/ana-ejemplo</uri>\n</author>\n"
    "<content type=\"text\"></content>\n<dcterms:language>es</dcterms:language>\n"
    "<summary>Un relato inventado.</summary>\n"
    "<dcterms:extent>23237 palabras</dcterms:extent><category scheme=\"https://www.textos.info/etiquetas\" "
    "term=\"novela-corta\" label=\"Novela corta\"/>\n<rights></rights>\n"
    "<link type=\"text/html\" title=\"Ver en textos.info\" href=\"https://www.textos.info/ana-ejemplo/el-faro-callado/ebook\" rel=\"alternate\"/>\n"
    "<link type=\"application/epub+zip\" href=\"https://www.textos.info/ana-ejemplo/el-faro-callado/epub\" rel=\"http://opds-spec.org/acquisition\"/>\n"
    "<link type=\"application/x-mobipocket-ebook\" href=\"https://www.textos.info/ana-ejemplo/el-faro-callado/mobi\" rel=\"http://opds-spec.org/acquisition\"/>\n"
    "<link type=\"application/pdf\" href=\"https://www.textos.info/ana-ejemplo/el-faro-callado/pdf\" rel=\"http://opds-spec.org/acquisition\"/>\n"
    "<link type=\"image/jpeg\" href=\"https://www.textos.info/img/0/530.jpg\" rel=\"http://opds-spec.org/image\"/>\n"
    "<link type=\"image/jpeg\" href=\"https://www.textos.info/img/0/530.jpg\" rel=\"http://opds-spec.org/image/thumbnail\"/>\n"
    "<link type=\"application/atom+xml;type=entry;profile=opds-catalog\" title=\"Informaci\xC3\xB3n\" "
    "href=\"https://www.textos.info/autores.atom&amp;\" rel=\"alternate\"/>\n"
    "</entry>\n</feed>\n";

// Ebooks libres et gratuits' newest as it came on 2026-11-25 (trimmed, the book made up): OPDS 1, no
// cover, the author as "Surname, Name", a Word file zipped as application/zip after the EPUB.
static const char FRENCH_REAL[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
    "<feed xmlns:app=\"http://www.w3.org/2007/app\" xmlns:opds=\"http://opds-spec.org/\" "
    "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns=\"http://www.w3.org/2005/Atom\" "
    "xmlns:dcterms=\"http://purl.org/dc/terms/\">\r\n"
    "  <title>Ebooks libres et gratuits - Derni\xC3\xA8res parutions</title>\r\n"
    "  <link type=\"application/atom+xml\" href=\"https://www.ebooksgratuits.com/opds/feed.php?page=1\" rel=\"next\" title=\"Next Page\"/>\r\n"
    "<entry>\r\n      <title>Le Phare tranquille</title>\r\n"
    "      <id>https://www.ebooksgratuits.com/details.php?book=3538</id>\r\n"
    "      <author>\r\n         <name>Exemple, Ana</name>\r\n"
    "         <uri>https://www.ebooksgratuits.com/ebooks.php?auteur=Exemple_Ana</uri>\r\n      </author>\r\n"
    "      <dcterms:language>fr</dcterms:language>\r\n"
    "      <link type=\"text/html\" href=\"https://www.ebooksgratuits.com/details.php?book=3538\" rel=\"alternate\" title=\"Voir sur ebooksgratuits.com\" />\r\n"
    "      <link type=\"application/atom+xml\" href=\"https://www.ebooksgratuits.com/opds/feed.php?mode=author&amp;id=411\" rel=\"related\" title=\"Du m\xC3\xAAme auteur\" />\r\n"
    "      <category term=\"Th\xC3\xA9\xC3\xA2tre\" label=\"Th\xC3\xA9\xC3\xA2tre\"/>\r\n"
    "      <content type=\"text\"><![CDATA[Une histoire invent\xC3\xA9\x65.<br /><br />\xC3\x89\x64ition Ebooks libres et gratuits.]]></content>\r\n"
    "      <link type=\"application/epub+zip\" href=\"https://www.ebooksgratuits.com/newsendbook.php?id=3538&amp;format=epub\" rel=\"http://opds-spec.org/acquisition\" />\r\n"
    "      <link type=\"application/x-mobipocket-ebook\" href=\"https://www.ebooksgratuits.com/newsendbook.php?id=3538&amp;format=mp\" rel=\"http://opds-spec.org/acquisition\" />\r\n"
    "      <link type=\"application/pdf\" href=\"https://www.ebooksgratuits.com/newsendbook.php?id=3538&amp;format=pdf\" rel=\"http://opds-spec.org/acquisition\" />\r\n"
    "      <link type=\"application/zip\" href=\"https://www.ebooksgratuits.com/newsendbook.php?id=3538&amp;format=so\" rel=\"http://opds-spec.org/acquisition\" />\r\n"
    "   </entry>\r\n</feed>\r\n";

// unglue.it's most popular with an EPUB as it came on 2026-11-25 (trimmed, the book made up): its
// download an "open-access" acquisition, keyword links to other lists, a next page.
static const char OPEN_REAL[] =
    "<?xml version='1.0' encoding='utf-8'?>\n"
    "<feed xmlns=\"http://www.w3.org/2005/Atom\" xmlns:dcterms=\"http://purl.org/dc/terms/\" "
    "xmlns:opds=\"http://opds-spec.org/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
    " <title>Unglue.it eBooks available in format: epub - Popular</title>\n"
    " <link href=\"https://unglue.it/api/opds/epub/?order_by=popular&amp;page=1\" rel=\"next\" title=\"Next 10\" "
    "type=\"application/atom+xml; profile=opds-catalog ;kind=acquisition; charset=utf-8\"/>\n"
    " <entry>\n  <title>\n   So You Want to Keep a Lighthouse\n  </title>\n"
    "  <id>\n   https://unglue.it/api/id/work/76348/\n  </id>\n"
    "  <link dcterms:rights=\"CC BY-SA\" href=\"https://unglue.it/download_ebook/249663/?feed=opds\" "
    "rel=\"http://opds-spec.org/acquisition/open-access\" type=\"application/epub+zip\"/>\n"
    "  <link href=\"https://files.unglue.it/cache/82/53/82532928e810f0efc5f2753cb1248278.jpg\" "
    "rel=\"http://opds-spec.org/image/thumbnail\" type=\"image/jpeg\"/>\n"
    "  <link href=\"https://files.unglue.it/cache/60/54/6054f6ad467e45646e2ffa5be0f9045b.jpg\" "
    "rel=\"http://opds-spec.org/image\" type=\"image/jpeg\"/>\n"
    "  <author>\n   <name>\n    Lio Arden\n   </name>\n  </author>\n"
    "  <dcterms:language>\n   en\n  </dcterms:language>\n"
    "  <content type=\"html\">\n   A made-up book.\n  </content>\n"
    "  <category term=\"Nonfiction\">\n  </category>\n"
    "  <link href=\"https://unglue.it/api/opds/kw.Nonfiction/?order_by=popular&amp;page=0\" rel=\"related\" "
    "title=\"Nonfiction\" type=\"application/atom+xml; profile=opds-catalog ;kind=acquisition; charset=utf-8\"/>\n"
    " </entry>\n</feed>\n";

static void testUrls(void)
{
    char url[512];
    CHECK(catalogUrl(CATALOG_POPULAR, CATALOG_POPULAR, NULL, url, sizeof(url)));
    CHECK_STR(url, "https://www.gutenberg.org/ebooks/search.opds/?sort_order=downloads");
    CHECK(catalogUrl(CATALOG_NEWEST, CATALOG_POPULAR, NULL, url, sizeof(url)));
    CHECK_STR(url, "https://standardebooks.org/feeds/atom/new-releases");
    CHECK(catalogUrl(CATALOG_SEARCH, CATALOG_POPULAR, "  pride & prejudice", url, sizeof(url)));
    CHECK_STR(url, "https://www.gutenberg.org/ebooks/search.opds/?query=pride+%26+prejudice");
    CHECK(catalogUrl(CATALOG_SEARCH, CATALOG_POPULAR, "Caf\xC3\xA9", url, sizeof(url)));
    CHECK_STR(url, "https://www.gutenberg.org/ebooks/search.opds/?query=Caf%C3%A9");
    CHECK(!catalogUrl(CATALOG_SEARCH, CATALOG_POPULAR, "   ", url, sizeof(url)));
    CHECK(!catalogUrl(CATALOG_SEARCH, CATALOG_POPULAR, NULL, url, sizeof(url)));
    CHECK(!catalogUrl(CATALOG_POPULAR, CATALOG_POPULAR, NULL, url, 20));
    CHECK_STR(catalogName(CATALOG_NEWEST), "Standard Ebooks: newest");
    CHECK(catalogUrl(CATALOG_SPANISH, CATALOG_POPULAR, NULL, url, sizeof(url)));
    CHECK_STR(url, "https://www.textos.info/populares.atom");
    CHECK(catalogUrl(CATALOG_FRENCH, CATALOG_POPULAR, "ignored", url, sizeof(url)));
    CHECK_STR(url, "https://www.ebooksgratuits.com/opds/feed.php");
    CHECK(catalogUrl(CATALOG_OPEN, CATALOG_POPULAR, NULL, url, sizeof(url)));
    CHECK_STR(url, "https://unglue.it/api/opds/epub/?order_by=popular");
    // Each catalog's own search, as its OpenSearch description gives it.
    CHECK(catalogUrl(CATALOG_SEARCH, CATALOG_NEWEST, "jane austen", url, sizeof(url)));
    CHECK_STR(url, "https://standardebooks.org/feeds/atom/all?query=jane+austen");
    CHECK(catalogUrl(CATALOG_SEARCH, CATALOG_SPANISH, "quijote", url, sizeof(url)));
    CHECK_STR(url, "https://www.textos.info/busqueda.atom&query=quijote");
    CHECK(catalogUrl(CATALOG_SEARCH, CATALOG_FRENCH, "mis\xC3\xA9rables", url, sizeof(url)));
    CHECK_STR(url, "https://www.ebooksgratuits.com/opds/feed.php?mode=search&query=mis%C3%A9rables");
    CHECK(catalogUrl(CATALOG_SEARCH, CATALOG_OPEN, "library", url, sizeof(url)));
    CHECK_STR(url, "https://unglue.it/api/opds/search/?q=library");
    CHECK(catalogUrl(CATALOG_SEARCH, (CatalogList)99, "x", url, sizeof(url)));  // Not a catalog: Gutenberg's.
    CHECK_STR(url, "https://www.gutenberg.org/ebooks/search.opds/?query=x");
    // Y searches the shown list's catalog; on a search, the one searched last.
    CHECK(catalogSearchOf(CATALOG_FRENCH, CATALOG_POPULAR) == CATALOG_FRENCH);
    CHECK(catalogSearchOf(CATALOG_SEARCH, CATALOG_SPANISH) == CATALOG_SPANISH);
    CHECK(catalogSearchOf(CATALOG_SEARCH, CATALOG_SEARCH) == CATALOG_POPULAR);
    CHECK_STR(catalogSearchName(CATALOG_OPEN), "unglue.it: search");
    CHECK_STR(catalogSearchLine(CATALOG_SPANISH), "Search textos.info");
    CHECK_STR(catalogSearchPrompt(CATALOG_POPULAR), "Search Project Gutenberg: a title, an author, a subject");
    CHECK_STR(catalogSearchHint(CATALOG_FRENCH), "Try other words from a title, in French.");
    for (int i = 0; i < CATALOG_SEARCH; i++)
        CHECK(catalogSearchName((CatalogList)i)[0] && catalogSearchPrompt((CatalogList)i)[0] &&
              catalogSearchLine((CatalogList)i)[0] && catalogSearchHint((CatalogList)i)[0]);
    // unglue.it's search: its next page without the words gets them back; the others' as they are.
    char next[512];
    CHECK(catalogNextPage("https://unglue.it/api/opds/search/?q=library",
                          "https://unglue.it/api/opds/search/?order_by=newest&page=1", next, sizeof(next)));
    CHECK_STR(next, "https://unglue.it/api/opds/search/?order_by=newest&page=1&q=library");
    CHECK(catalogNextPage("https://unglue.it/api/opds/search/?q=a+b&order_by=newest&page=1",
                          "https://unglue.it/api/opds/search/?order_by=newest&page=2", next, sizeof(next)));
    CHECK_STR(next, "https://unglue.it/api/opds/search/?order_by=newest&page=2&q=a+b");
    CHECK(catalogNextPage("https://standardebooks.org/feeds/atom/all?query=love",
                          "https://standardebooks.org/feeds/atom/all?query=love&page=2&per-page=12", next, sizeof(next)));
    CHECK_STR(next, "https://standardebooks.org/feeds/atom/all?query=love&page=2&per-page=12");
    CHECK(catalogNextPage("https://unglue.it/api/opds/search/?q=x", "", next, sizeof(next)) && !next[0]);
    CHECK(!catalogNextPage("https://a/", "https://a/very/long/next", next, 8));
    CHECK_STR(catalogName(CATALOG_FRENCH), "Ebooks libres et gratuits: newest (French)");
    CHECK_STR(catalogShowLine(CATALOG_SPANISH), "Show textos.info's most popular, in Spanish");
    CHECK_STR(catalogShowLine(CATALOG_SEARCH), "Show your last search");
    CHECK_STR(catalogShowLine((CatalogList)99), "Show Project Gutenberg's most popular");
    for (int i = 0; i < CATALOG_COUNT; i++)
        CHECK(catalogName((CatalogList)i)[0] && catalogShowLine((CatalogList)i)[0]);
}

static void testPages(void)
{
    OpdsFeed f;
    OpdsBook *books;
    CHECK(opdsParse(GUTENBERG_PAGE, sizeof(GUTENBERG_PAGE) - 1, "https://www.gutenberg.org/ebooks/search.opds/", &f));
    int n = catalogBooks(&f, &books);
    CHECK(n == 2);  // Not "Sort Alphabetically".
    if (n == 2) {
        CHECK_STR(books[0].title, "A Made-up Novel");
        CHECK_STR(books[0].author, "Ana Example");
        CHECK_STR(books[0].acquire, "https://www.gutenberg.org/ebooks/84.epub3.images");
        CHECK_STR(books[0].cover, "https://www.gutenberg.org/cache/epub/84/pg84.cover.medium.jpg");
        CHECK_STR(books[0].ext, ".epub");
        CHECK_STR(books[1].id, "pg:1342");
        char name[128];
        catalogFileName(&books[1], name, sizeof(name));
        CHECK_STR(name, "Cafe Stories - Lio Arden.epub");
    }
    CHECK_STR(f.next, "https://www.gutenberg.org/ebooks/search.opds/?sort_order=downloads&start_index=26");
    free(books);
    opdsFeedFree(&f);

    CHECK(opdsParse(NEWEST_PAGE, sizeof(NEWEST_PAGE) - 1, "https://standardebooks.org/feeds/atom/new-releases", &f));
    n = catalogBooks(&f, &books);
    CHECK(n == 1);
    if (n == 1) {
        CHECK_STR(books[0].title, "The Quiet Lighthouse");
        CHECK_STR(books[0].author, "Ana Example");
        CHECK_STR(books[0].acquire, "https://standardebooks.org/x/a.epub?source=feed");  // The first, compatible EPUB.
        CHECK(books[0].size == 875588);
        CHECK_STR(books[0].cover, "https://standardebooks.org/x/cover-thumbnail.jpg");
    }
    free(books);
    opdsFeedFree(&f);
}

// The feeds as they really come, and Gutenberg's maintenance page.
static void testRealAnswers(void)
{
    OpdsFeed f;
    OpdsBook *books;
    CHECK(opdsParse(NEWEST_REAL, sizeof(NEWEST_REAL) - 1, "https://standardebooks.org/feeds/atom/new-releases", &f));
    CHECK_STR(f.title, "Standard Ebooks - Newest Ebooks");
    int n = catalogBooks(&f, &books);
    CHECK(n == 1);
    if (n == 1) {
        CHECK_STR(books[0].title, "The Salt Ledger");
        CHECK_STR(books[0].author, "Ana Example");
        CHECK_STR(books[0].acquire, "https://standardebooks.org/ebooks/ana-example/the-salt-ledger/downloads/"
                                    "ana-example_the-salt-ledger.epub?source=feed");
        CHECK(books[0].size == 875588);
        CHECK_STR(books[0].cover,
                  "https://standardebooks.org/ebooks/ana-example/the-salt-ledger/downloads/cover-thumbnail.jpg");
        CHECK_STR(books[0].genre, "Fiction");
        CHECK_STR(books[0].ext, ".epub");
    }
    free(books);
    opdsFeedFree(&f);

    // Not a feed: said as a problem, never shown as an empty list.
    CHECK(!opdsParse(GUTENBERG_DOWN, sizeof(GUTENBERG_DOWN) - 1, "https://www.gutenberg.org/ebooks/search.opds/", &f));
    CHECK(f.books == NULL && f.feeds == NULL && f.book_count == 0);
    CHECK(opdsBusyStatus(503) && opdsBusyStatus(502) && opdsBusyStatus(504));
    CHECK(!opdsBusyStatus(200) && !opdsBusyStatus(404) && !opdsBusyStatus(500) && !opdsBusyStatus(0));
}

// The Spanish and French catalogs as they really come.
static void testOtherLanguages(void)
{
    OpdsFeed f;
    OpdsBook *books;
    CHECK(opdsParse(SPANISH_REAL, sizeof(SPANISH_REAL) - 1, "https://www.textos.info/populares.atom", &f));
    int n = catalogBooks(&f, &books);
    CHECK(n == 1);
    if (n == 1) {
        CHECK_STR(books[0].title, "El Faro Callado");
        CHECK_STR(books[0].author, "Ana Ejemplo");
        CHECK_STR(books[0].acquire, "https://www.textos.info/ana-ejemplo/el-faro-callado/epub");  // The EPUB, not the PDF.
        CHECK_STR(books[0].ext, ".epub");
        CHECK_STR(books[0].cover, "https://www.textos.info/img/0/530.jpg");
        CHECK_STR(books[0].genre, "Novela corta");
    }
    CHECK_STR(f.next, "https://www.textos.info/populares.atom&page=2");
    CHECK(f.feed_count == 0);  // Its "Informacion" link isn't a list of its own.
    free(books);
    opdsFeedFree(&f);

    CHECK(opdsParse(FRENCH_REAL, sizeof(FRENCH_REAL) - 1, "https://www.ebooksgratuits.com/opds/feed.php", &f));
    n = catalogBooks(&f, &books);
    CHECK(n == 1);
    if (n == 1) {
        CHECK_STR(books[0].title, "Le Phare tranquille");
        CHECK_STR(books[0].author, "Ana Exemple");  // As written; its "Surname, Name" kept for sorting.
        CHECK_STR(books[0].author_sort, "Exemple, Ana");
        CHECK_STR(books[0].acquire, "https://www.ebooksgratuits.com/newsendbook.php?id=3538&format=epub");
        CHECK_STR(books[0].ext, ".epub");  // Not the zipped Word file.
        CHECK_STR(books[0].cover, "");     // None: the book's own cover, once read.
        CHECK_STR(books[0].genre, "Th\xC3\xA9\xC3\xA2tre");
        char name[128];
        catalogFileName(&books[0], name, sizeof(name));
        CHECK_STR(name, "Le Phare tranquille - Ana Exemple.epub");
    }
    CHECK_STR(f.next, "https://www.ebooksgratuits.com/opds/feed.php?page=1");
    free(books);
    opdsFeedFree(&f);

    CHECK(opdsParse(OPEN_REAL, sizeof(OPEN_REAL) - 1, "https://unglue.it/api/opds/epub/?order_by=popular", &f));
    n = catalogBooks(&f, &books);
    CHECK(n == 1);
    if (n == 1) {
        CHECK_STR(books[0].title, "So You Want to Keep a Lighthouse");
        CHECK_STR(books[0].author, "Lio Arden");
        CHECK_STR(books[0].author_sort, "");
        CHECK_STR(books[0].acquire, "https://unglue.it/download_ebook/249663/?feed=opds");  // open-access: read here.
        CHECK_STR(books[0].ext, ".epub");
        CHECK_STR(books[0].cover, "https://files.unglue.it/cache/82/53/82532928e810f0efc5f2753cb1248278.jpg");
        CHECK_STR(books[0].genre, "Nonfiction");
    }
    CHECK_STR(f.next, "https://unglue.it/api/opds/epub/?order_by=popular&page=1");
    CHECK(f.feed_count == 0);  // Its keyword links lead to lists, not shown as books.
    free(books);
    opdsFeedFree(&f);
}

// "Surname, Name" turned around; everything else left as it is.
static void testAuthors(void)
{
    char out[64] = "unset";
    CHECK(catalogAuthorAsWritten("Hugo, Victor", out, sizeof(out)));
    CHECK_STR(out, "Victor Hugo");
    CHECK(catalogAuthorAsWritten("Goncourt, Edmond de", out, sizeof(out)));
    CHECK_STR(out, "Edmond de Goncourt");
    CHECK(catalogAuthorAsWritten("Doyle ,  Arthur Conan", out, sizeof(out)));
    CHECK_STR(out, "Arthur Conan Doyle");
    strcpy(out, "unset");
    CHECK(!catalogAuthorAsWritten("Victor Hugo", out, sizeof(out)));           // No comma.
    CHECK(!catalogAuthorAsWritten("Hugo, Victor, 1802-1885", out, sizeof(out)));  // Several.
    CHECK(!catalogAuthorAsWritten("Dumas, Alexandre et Maquet, Auguste", out, sizeof(out)));
    CHECK(!catalogAuthorAsWritten("King, Jr.", out, sizeof(out)));             // A suffix, not a name.
    CHECK(!catalogAuthorAsWritten("Hugo, 1802", out, sizeof(out)));
    CHECK(!catalogAuthorAsWritten(", Victor", out, sizeof(out)) && !catalogAuthorAsWritten("Hugo,", out, sizeof(out)));
    CHECK_STR(out, "unset");
    CHECK(!catalogAuthorAsWritten("Hugo, Victor", out, 6));  // Doesn't fit.
}

void testCatalogs(void)
{
    testUrls();
    testPages();
    testRealAnswers();
    testOtherLanguages();
    testAuthors();
}
