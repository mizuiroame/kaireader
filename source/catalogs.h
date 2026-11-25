// The free catalogs KaiReader browses: Project Gutenberg (its most popular
// books, OPDS 1), Standard Ebooks (its newest, the Atom feed it gives to all;
// the rest is for its patrons), textos.info (Spanish: its most popular, OPDS 1),
// Ebooks libres et gratuits (French: its newest, OPDS 1; at most 50 downloads a
// day each, one at a time, as KaiReader does them) and unglue.it (books under
// Creative Commons licenses, its most popular, OPDS 1); and each one's search.
// Public-domain or freely licensed books only. Their lists' addresses, and a
// page's books with what's needed to download them. Pure logic, tested on the
// PC; netjob.c fetches the pages.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "opds.h"

typedef enum {
    CATALOG_POPULAR,  // Project Gutenberg's most downloaded.
    CATALOG_NEWEST,   // Standard Ebooks' newest.
    CATALOG_SPANISH,  // textos.info's most popular, in Spanish.
    CATALOG_FRENCH,   // Ebooks libres et gratuits' newest, in French.
    CATALOG_OPEN,     // unglue.it's most popular, under Creative Commons.
    CATALOG_SEARCH,   // A search of one of them: the last list, there once there's been one.
    CATALOG_COUNT,
} CatalogList;

#define CATALOG_QUERY_MAX 128

// The list's name ("Project Gutenberg: most popular"), English for T(). A search's is catalogSearchName.
const char *catalogName(CatalogList list);
// The + menu's line that goes to list ("Show Standard Ebooks' newest"), English for T().
const char *catalogShowLine(CatalogList list);

// Whose search Y opens: the shown list's catalog, or on a search's list, the one searched last.
CatalogList catalogSearchOf(CatalogList shown, CatalogList last);
// For the catalog a list browses: its search list's name (as catalogName), the keyboard's
// title, the + menu's line, and what to try when nothing's found (English for T()).
const char *catalogSearchName(CatalogList in);
const char *catalogSearchPrompt(CatalogList in);
const char *catalogSearchLine(CatalogList in);
const char *catalogSearchHint(CatalogList in);

// The list's first page: for a search, of query (encoded) in search_in's catalog. False if
// it doesn't fit, or a search without words.
bool catalogUrl(CatalogList list, CatalogList search_in, const char *query, char *out, size_t size);
// The next page after asked, as its feed says (next): what's missing from it put back. False if it doesn't fit.
bool catalogNextPage(const char *asked, const char *next, char *out, size_t size);
// "Hugo, Victor" as written: "Victor Hugo". False (out unset) when it isn't "Surname, Name":
// no comma, several, a suffix ("King, Jr.") or years after it.
bool catalogAuthorAsWritten(const char *author, char *out, size_t size);
// A page's books, in its order: the feed's own (with downloads, as Standard Ebooks'),
// and Gutenberg's (entries leading to /ebooks/N.opds: its EPUB with pictures, and its
// cover). Ebooks libres et gratuits' authors as written, their "Surname, Name" in
// author_sort. *out is malloc'd; how many.
int catalogBooks(const OpdsFeed *page, OpdsBook **out);
// The name a downloaded book gets in the card's books folder: "Title - Author.epub", plain.
void catalogFileName(const OpdsBook *b, char *out, size_t size);
