// The free catalogs KaiReader browses: Project Gutenberg (its most popular
// books and its search, OPDS 1), Standard Ebooks (its newest, the Atom feed it
// gives to all; the rest is for its patrons), textos.info (Spanish: its most
// popular, OPDS 1) and Ebooks libres et gratuits (French: its newest, OPDS 1;
// at most 50 downloads a day each, one at a time, as KaiReader does them).
// Public-domain books only. Their lists' addresses, and a page's books with what's needed to
// download them. Pure logic, tested on the PC; netjob.c fetches the pages.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "opds.h"

typedef enum {
    CATALOG_POPULAR,  // Project Gutenberg's most downloaded.
    CATALOG_NEWEST,   // Standard Ebooks' newest.
    CATALOG_SPANISH,  // textos.info's most popular, in Spanish.
    CATALOG_FRENCH,   // Ebooks libres et gratuits' newest, in French.
    CATALOG_SEARCH,   // A search of Project Gutenberg: the last list, there once there's been one.
    CATALOG_COUNT,
} CatalogList;

#define CATALOG_QUERY_MAX 128

// The list's name ("Project Gutenberg: most popular"), English for T().
const char *catalogName(CatalogList list);
// The + menu's line that goes to list ("Show Standard Ebooks' newest"), English for T().
const char *catalogShowLine(CatalogList list);
// The list's first page: for a search, of query (encoded). False if it doesn't fit, or a search without words.
bool catalogUrl(CatalogList list, const char *query, char *out, size_t size);
// A page's books, in its order: the feed's own (with downloads, as Standard Ebooks'),
// and Gutenberg's (entries leading to /ebooks/N.opds: its EPUB with pictures, and its
// cover). *out is malloc'd; how many.
int catalogBooks(const OpdsFeed *page, OpdsBook **out);
// The name a downloaded book gets in the card's books folder: "Title - Author.epub", plain.
void catalogFileName(const OpdsBook *b, char *out, size_t size);
