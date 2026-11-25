#include "catalogs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netlist.h"

#define GUTENBERG "https://www.gutenberg.org"

static const char *const NAMES[CATALOG_COUNT] = {
    "Project Gutenberg: most popular",
    "Standard Ebooks: newest",
    "Project Gutenberg: search",
};

const char *catalogName(CatalogList list)
{
    return list >= 0 && list < CATALOG_COUNT ? NAMES[list] : NAMES[0];
}

// query as a URL's query value: letters, digits and -._~ as they are, spaces as +, the rest %XX.
static bool encode(const char *query, char *out, size_t size)
{
    size_t o = 0;
    for (const unsigned char *s = (const unsigned char *)query; *s; s++) {
        if (o + 4 > size)
            return false;
        if (isalnum(*s) || strchr("-._~", *s))
            out[o++] = (char)*s;
        else if (*s == ' ')
            out[o++] = '+';
        else
            o += (size_t)snprintf(out + o, size - o, "%%%02X", *s);
    }
    out[o] = '\0';
    return true;
}

bool catalogUrl(CatalogList list, const char *query, char *out, size_t size)
{
    int n;
    if (list == CATALOG_POPULAR) {
        n = snprintf(out, size, "%s", GUTENBERG "/ebooks/search.opds/?sort_order=downloads");
    } else if (list == CATALOG_NEWEST) {
        n = snprintf(out, size, "%s", "https://standardebooks.org/feeds/atom/new-releases");
    } else if (list == CATALOG_SEARCH) {
        char words[CATALOG_QUERY_MAX * 3 + 1];
        while (query && *query == ' ')
            query++;
        if (!query || !*query || !encode(query, words, sizeof(words)))
            return false;
        n = snprintf(out, size, GUTENBERG "/ebooks/search.opds/?query=%s", words);
    } else {
        return false;
    }
    return n > 0 && (size_t)n < size;
}

// Gutenberg's book number in a link to its page ("https://www.gutenberg.org/ebooks/84.opds"); -1 if it isn't one.
static long gutenbergNumber(const char *url)
{
    const char *at = strstr(url, "gutenberg.org/ebooks/");
    if (!at)
        return -1;
    at += strlen("gutenberg.org/ebooks/");
    char *end;
    long n = strtol(at, &end, 10);
    return end != at && n > 0 && !strcmp(end, ".opds") ? n : -1;
}

int catalogBooks(const OpdsFeed *page, OpdsBook **out)
{
    *out = calloc((size_t)(page->book_count + page->feed_count + 1), sizeof(**out));
    if (!*out)
        return 0;
    int n = 0;
    for (int i = 0; i < page->book_count; i++)
        (*out)[n++] = page->books[i];
    for (int i = 0; i < page->feed_count; i++) {
        const OpdsLink *l = &page->feeds[i];
        long number = gutenbergNumber(l->url);
        if (number < 0 || !l->title[0])
            continue;  // A list, not a book ("Sort by title", an author's page...).
        OpdsBook *b = &(*out)[n++];
        memset(b, 0, sizeof(*b));
        snprintf(b->id, sizeof(b->id), "pg:%ld", number);
        snprintf(b->title, sizeof(b->title), "%s", l->title);
        snprintf(b->author, sizeof(b->author), "%s", l->summary);
        snprintf(b->acquire, sizeof(b->acquire), GUTENBERG "/ebooks/%ld.epub3.images", number);
        snprintf(b->ext, sizeof(b->ext), "%s", ".epub");
        snprintf(b->cover, sizeof(b->cover), GUTENBERG "/cache/epub/%ld/pg%ld.cover.medium.jpg", number, number);
        b->size = -1;
    }
    return n;
}

void catalogFileName(const OpdsBook *b, char *out, size_t size)
{
    char name[OPDS_TEXT_MAX * 2 + OPDS_EXT_MAX + 8];
    snprintf(name, sizeof(name), "%s%s%s%s", b->title[0] ? b->title : "Book", b->author[0] ? " - " : "", b->author,
             b->ext[0] ? b->ext : ".epub");
    netPlainName(name, false, out, size);
}
