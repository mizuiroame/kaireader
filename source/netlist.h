// The network folder in the library: what of a folder's listing shows (its
// folders and books, not hidden ones, folders first, each by name), moving
// between its folders, and where a book read from it is kept on the card.
// Pure logic, tested on the PC.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "dav.h"

// Keeps the folders and the books (docKnownName) of entries, without hidden
// ones (".name"), folders first, each by name; how many are left.
size_t netListBooks(DavEntry *entries, size_t count);

// A name the console's card lists: plain ASCII (accents dropped, "é" -> "e"),
// without \ / : * ? " < > | or a leading dot. When characters had to be left
// out (Japanese, say), a short code of the whole name is added, so two such
// names don't become one; a file keeps its extension.
void netPlainName(const char *name, bool is_dir, char *out, size_t size);

// Where a book of the folder is kept: books_folder, "/Network/", then its
// path, each part a plain name ("dav:/Café/Mon livre.epub" ->
// "sdmc:/books/Network/Cafe/Mon livre.epub"). False if it isn't a location
// of a book, or is too long.
bool netLocalPath(const char *books_folder, const char *location, char *out, size_t size);

// The folder a location is in ("dav:/A/b.epub" -> "dav:/A", "dav:/A" -> "dav:/"). False at the top.
bool netParent(const char *location, char *out, size_t size);
// An entry of the folder at location ("dav:/" + "A" -> "dav:/A"). False if too long.
bool netChild(const char *location, const char *name, char *out, size_t size);
// The last part of a location, "" at the top.
const char *netLastName(const char *location);
