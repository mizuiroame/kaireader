// Bookmarks: places in books, kept in bookmarks.txt next to the NRO (a line
// each: path, chapter, fraction, when made). A place is a DocPlace, so a
// bookmark stays on its text after a new text size or turn.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "doc.h"
#include "shelf.h"

#define BOOKMARKS_PER_BOOK 64
#define BOOKMARKS_MAX 2000  // In all books: a bigger file isn't KaiReader's.

typedef struct {
    char path[SHELF_PATH_MAX];
    DocPlace place;
    int64_t made;  // Seconds since 1970.
} Bookmark;

typedef struct {
    Bookmark *items;
    int count, cap;
} Bookmarks;

void bookmarksFree(Bookmarks *b);
// Reads the file over an empty list (none if there's no file). False only if it's there and unreadable.
bool bookmarksLoad(Bookmarks *b, const char *file);
// Writes it whole (to a new file first, then in place of the old).
bool bookmarksSave(const Bookmarks *b, const char *file);

// The book's bookmarks, in reading order: up to max indexes into b->items; how many.
int bookmarksOf(const Bookmarks *b, const char *path, int *out, int max);
// The book's bookmark at place (the same chapter, the same spot give or take
// a hair), or -1.
int bookmarksFind(const Bookmarks *b, const char *path, DocPlace place);
// Adds one (none twice at the same place, at most BOOKMARKS_PER_BOOK a book). False if it can't.
bool bookmarksAdd(Bookmarks *b, const char *path, DocPlace place, int64_t made);
void bookmarksRemove(Bookmarks *b, int index);
