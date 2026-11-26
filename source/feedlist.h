// The news feeds you follow (feeds.txt in KaiReader's folder: one a line, its
// address, whether its certificate is checked, and its name), and the read
// marks of their articles: those are the news shelf's (shelf-news.txt), and a
// mark is forgotten 30 days after it was read once its article isn't kept any
// more. Pure file work, tested on the PC.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "feeds.h"
#include "shelf.h"

#define FEEDLIST_FILE "feeds.txt"
#define FEEDLIST_MAX 64
#define NEWS_PREFIX "news:"          // An article in the library: "news:<feed hash>/<article hash>".
#define NEWS_MARK_DAYS 30            // A read mark kept this long after its article's gone.

typedef struct {
    char url[FEED_URL_MAX];
    char name[FEED_TEXT_MAX];
    bool any_certificate;            // Its https certificate isn't checked (a server of your own).
    char hash[FEED_HASH_LEN + 1];    // Its url's: its files' name on the card.
} FeedEntry;

typedef struct {
    FeedEntry items[FEEDLIST_MAX];
    int count;
} FeedList;

// Reads the list (empty if there's no file). False only if it's there and can't be read.
bool feedListLoad(FeedList *l, const char *file);
// Writes it whole (to a new file first, then in place of the old).
bool feedListSave(const FeedList *l, const char *file);
// Adds a feed (its name with tabs and line breaks made spaces): its index; -1 if the list is
// full, or the index it has already.
int feedListAdd(FeedList *l, const char *url, const char *name, bool *added);
void feedListRemove(FeedList *l, int index);
void feedListRename(FeedList *l, int index, const char *name);
// The feed with that hash; -1 if none.
int feedListFind(const FeedList *l, const char *hash);

// An article's path in the library, and back: its feed's hash and its own. False if it isn't one.
bool newsPath(const char *feed_hash, const char *item_id, char *out, size_t size);
bool newsPathParts(const char *path, char feed_hash[FEED_HASH_LEN + 1], char item_hash[FEED_HASH_LEN + 1]);

// The marks of articles gone (not present) and read more than NEWS_MARK_DAYS before now are
// forgotten: shelfSave won't keep them. How many.
int newsForgetMarks(Shelf *s, int64_t now);
