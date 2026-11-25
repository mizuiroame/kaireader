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

static void testUrls(void)
{
    char url[512];
    CHECK(catalogUrl(CATALOG_POPULAR, NULL, url, sizeof(url)));
    CHECK_STR(url, "https://www.gutenberg.org/ebooks/search.opds/?sort_order=downloads");
    CHECK(catalogUrl(CATALOG_NEWEST, NULL, url, sizeof(url)));
    CHECK_STR(url, "https://standardebooks.org/feeds/atom/new-releases");
    CHECK(catalogUrl(CATALOG_SEARCH, "  pride & prejudice", url, sizeof(url)));
    CHECK_STR(url, "https://www.gutenberg.org/ebooks/search.opds/?query=pride+%26+prejudice");
    CHECK(catalogUrl(CATALOG_SEARCH, "Caf\xC3\xA9", url, sizeof(url)));
    CHECK_STR(url, "https://www.gutenberg.org/ebooks/search.opds/?query=Caf%C3%A9");
    CHECK(!catalogUrl(CATALOG_SEARCH, "   ", url, sizeof(url)));
    CHECK(!catalogUrl(CATALOG_SEARCH, NULL, url, sizeof(url)));
    CHECK(!catalogUrl(CATALOG_POPULAR, NULL, url, 20));
    CHECK_STR(catalogName(CATALOG_NEWEST), "Standard Ebooks: newest");
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

void testCatalogs(void)
{
    testUrls();
    testPages();
}
