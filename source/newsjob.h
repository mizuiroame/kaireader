// The news' slow work, off the screen's thread: checking the feeds (each
// asked in turn, its new articles merged with the ones kept on the card,
// <folder>/<feed hash>.xml), finding the feed a typed address leads to, and
// fetching an article's whole page. One of each at a time; the app asks each
// frame how they go.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "feeds.h"
#include "netjob.h"
#include "shelf.h"

typedef struct {
    char url[FEED_URL_MAX];
    char hash[FEED_HASH_LEN + 1];
    bool any_certificate;
} NewsJobFeed;

// An article for the library (its text stays in its feed's file).
typedef struct {
    char path[SHELF_PATH_MAX];      // "news:<feed hash>/<article hash>".
    char title[FEED_TEXT_MAX];
    char image[FEED_URL_MAX];
    int64_t date;
    int feed;                       // Its feed, as given.
    int seq;                        // Its place in its feed's round (the same dates keep it).
} NewsArticle;

typedef struct {
    char hash[FEED_HASH_LEN + 1];   // Which feed.
    char title[FEED_TEXT_MAX];      // The feed's own name for itself.
    bool ok;                        // Read now (or, before asking, from the card).
    char why[NETJOB_WHY_MAX];       // Why not.
} NewsFeedState;

// Reads the feeds kept in folder at once (handed over first), then asks each site for its new
// articles. False if a check runs already.
bool newsJobCheck(const NewsJobFeed *feeds, int count, const char *folder);
bool newsJobChecking(int *done, int *count);
// What's come, once: the articles of every feed, newest first (malloc'd, the caller frees them),
// each feed's state (as many as were given, in *feed_count; malloc'd), and whether the sites were
// asked yet.
bool newsJobTakeCheck(NewsArticle **articles, int *count, NewsFeedState **feeds, int *feed_count, bool *asked);

// The feed at url, or the one a page there links to: its address and name. False if one runs already.
bool newsJobFind(const char *url);
bool newsJobFinding(void);
bool newsJobTakeFind(char *url, size_t url_size, char *title, size_t title_size, bool *ok, char *why,
                     size_t why_size);

// An article's page on its site: its main part, as HTML. False if one runs already.
bool newsJobFull(const char *link, bool any_certificate);
bool newsJobTakeFull(char **html, bool *ok, char *why, size_t why_size);

// Waits for them to end. At exit (before the network stops).
void newsJobStop(void);
