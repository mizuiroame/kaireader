// OPDS 1.x catalogs (Atom feeds), as Calibre-Web, Kavita and Komga serve
// them: a feed's books (each with the link KaiReader can read best, its
// cover, title, author and genre), the feeds it leads to (its subsections),
// and its next page. Pure: parses text and resolves links; opdsnet.c asks
// the server. Tested on the PC.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OPDS_URL_MAX 1024
#define OPDS_TEXT_MAX 160
#define OPDS_EXT_MAX 8
#define OPDS_PREFIX "opds:"  // A book of the server in the library: "opds:" and its download link.

typedef struct {
    char id[OPDS_URL_MAX];        // The entry's id (urn:...), to know it again on another page.
    char title[OPDS_TEXT_MAX];
    char author[OPDS_TEXT_MAX];
    char author_sort[OPDS_TEXT_MAX];  // "Surname, Name" when author was turned around for showing; "" if not.
    char genre[OPDS_TEXT_MAX];    // Its first category.
    char acquire[OPDS_URL_MAX];   // The download link of the kind read best (EPUB first), absolute.
    char ext[OPDS_EXT_MAX];       // Its kind as an extension: ".epub", ".pdf", ".cbz"...
    char cover[OPDS_URL_MAX];     // Its thumbnail (or cover image), absolute; "" if none.
    int64_t size;                 // Bytes, if the link says; -1 if not.
} OpdsBook;

// A feed an entry leads to (a list, or a book's own page): absolute, with the entry's words.
typedef struct {
    char url[OPDS_URL_MAX];
    char title[OPDS_TEXT_MAX];
    char summary[OPDS_TEXT_MAX];  // Its content's text (Gutenberg puts the author there).
} OpdsLink;

typedef struct {
    OpdsBook *books;
    int book_count;
    OpdsLink *feeds;  // The feeds its entries lead to (navigation).
    int feed_count;
    char next[OPDS_URL_MAX];      // Its next page; "" if it's the last.
    char title[OPDS_TEXT_MAX];
} OpdsFeed;

// Parses a feed fetched from base_url. False if it isn't an Atom feed.
bool opdsParse(const char *xml, size_t size, const char *base_url, OpdsFeed *out);
void opdsFeedFree(OpdsFeed *f);

// href (absolute, from the host's root, or relative) against base, into out. False if it doesn't fit.
bool opdsResolve(const char *base, const char *href, char *out, size_t size);

// True for an HTTP status that says the server is down for now (502, 503, 504:
// maintenance, or overloaded), not that the address is wrong.
bool opdsBusyStatus(long status);

// The extension of a kind of book KaiReader opens, from its MIME type (or the
// link's own name); NULL for a kind it doesn't.
const char *opdsExtensionFor(const char *mime, const char *href);
