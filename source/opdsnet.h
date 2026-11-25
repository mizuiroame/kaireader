// The book server (an OPDS catalog: Calibre-Web, Kavita, Komga...): its
// account, kept for every thread, and what's asked of it over HTTP(S). Its
// account is a DavAccount (dav.h: an http(s) address, a user and a
// password, written to its file the same way); the address is the catalog's.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "dav.h"
#include "doc.h"
#include "opds.h"

#define OPDS_ACCOUNT_FILE "server.txt"

void opdsUseAccount(const DavAccount *a);
// Off: the server's https certificate isn't checked (its own). Only for the server's address.
void opdsCheckCertificate(bool check);
// True when url is the server's and its certificate isn't to be checked.
bool opdsAnyCertificate(const char *url);
void opdsGetAccount(DavAccount *out);
bool opdsConfigured(void);
void opdsLoadAccount(const char *path);
bool opdsSaveAccount(const char *path);

// A feed of the server, parsed. False with why (for the person) if it can't be.
bool opdsFetchFeed(const char *url, OpdsFeed *out, char *why, size_t why_size);
// The catalog at address, or under it where the servers keep theirs (/opds,
// /opds/v1.2/catalog...): its address into catalog. False with why.
bool opdsFindCatalog(const char *address, char *catalog, size_t size, char *why, size_t why_size);
// Asks the server for its catalog: what it says, as a sentence. True if it answered with one.
bool opdsTest(char *out, size_t size);
// The Authorization header for url: the server's sign-in if url is on the server, "" otherwise
// (or without a user). False if it doesn't fit.
bool opdsAuthorization(const char *url, char *out, size_t size);

// Where a server's book ("opds:<link>#.ext") is kept on the card:
// books_folder "/.opds/", its title as a plain name, its kind. False if it doesn't fit.
bool opdsLocalPath(const char *books_folder, const char *path, const char *title, char *out, size_t size);
// A server's book's download link (its path without "opds:" and its "#.ext").
bool opdsLinkOf(const char *path, char *out, size_t size);

// Each server book's thumbnail, as found in the catalog: kept for the reading thread.
void opdsRememberCover(const char *path, const char *url);
void opdsForgetCovers(void);
// The reading thread's fetcher (reading.h): a server book's thumbnail, fitted.
bool opdsFetchCover(const char *path, int w, int h, DocImage *out);
