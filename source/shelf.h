// The books KaiReader knows: the ones found in the books folder, and for each
// where it was left, how far in, and when it was last read. Kept in a text
// file next to the NRO (shelf.txt), one book a line. Pure file work, tested
// on the PC.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "doc.h"

#define SHELF_PATH_MAX 512
#define SHELF_TEXT_MAX 160
#define SHELF_MAX_BOOKS 4000
#define SHELF_SCAN_DEPTH 4  // Folders inside the books folder, and folders in those...

typedef struct {
    char path[SHELF_PATH_MAX];
    char title[SHELF_TEXT_MAX];   // What the book says, once opened; empty before.
    char author[SHELF_TEXT_MAX];
    char author_sort[SHELF_TEXT_MAX];  // "Example, Ana", when the book says it; else empty.
    char genre[SHELF_TEXT_MAX];        // The book's subject (an EPUB's first, a PDF's); empty if none.
    int64_t size;                 // Of the file, when last seen.
    DocPlace place;               // Where it was left.
    int percent;                  // How far in, 0-100; -1 if never opened.
    int64_t last_read;            // Seconds since 1970; 0 if never opened.
    bool present;                 // Found by the last scan.
    bool favorite;                // Marked (+ menu, or a long touch on its cover).
    bool on_card;                 // Free books: downloaded to the card already (never saved).
} ShelfBook;

typedef struct {
    ShelfBook *items;
    int count, cap;
} Shelf;

void shelfFree(Shelf *s);

// Reads a shelf file (an empty shelf if there's none). False only if it's there and can't be read.
bool shelfLoad(Shelf *s, const char *file);
// Writes it whole (to a new file first, then in place of the old). Books never opened
// and not present aren't kept.
bool shelfSave(const Shelf *s, const char *file);

// Looks through folder (and its folders, SHELF_SCAN_DEPTH deep) for books.
// New ones are added; every book is marked present or not. Hidden files and
// folders (".name") are skipped. Returns how many are present.
int shelfScan(Shelf *s, const char *folder);

ShelfBook *shelfFind(Shelf *s, const char *path);
// Adds a book by path (present), or returns the one there. NULL if the shelf is full.
ShelfBook *shelfAdd(Shelf *s, const char *path);

typedef enum {
    SHELF_SORT_RECENT,  // The ones being read, most recent first, then the rest by title.
    SHELF_SORT_TITLE,
    SHELF_SORT_AUTHOR,  // By surname when the book gives it ("Example, Ana"), then by title.
    SHELF_SORT_GENRE,   // By genre, then author, then title; books without one at the end.
    SHELF_SORT_FAVORITES,  // Only the favorites, by title. (Last: settings.txt keeps the number.)
    SHELF_SORT_COUNT,
} ShelfSort;

// The present books in the library's order (Favorites: only those). Fills order with indexes; how many.
int shelfOrder(const Shelf *s, ShelfSort sort, int *order, int max);
// The group a book is in for a sort (its author or genre, to show above it): empty for
// the sorts without groups, and for books without an author or genre.
void shelfGroupOf(const ShelfBook *b, ShelfSort sort, char *out, size_t size);

// The name to show: the book's title, or its file's name without the extension.
void shelfDisplayName(const ShelfBook *b, char *out, size_t size);

// After reading: where, how far, now.
void shelfMarkRead(ShelfBook *b, DocPlace place, int percent, int64_t now);
// The title and author the book gives (on opening it).
void shelfSetInfo(ShelfBook *b, const char *title, const char *author);
// Its genre and the author's sort name (from its cover's read): empty ones leave what's kept.
void shelfSetDetails(ShelfBook *b, const char *genre, const char *author_sort);
