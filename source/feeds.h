// News feeds: RSS 2.0, RSS 1.0 (RDF) and Atom 1.0, read into articles (an
// id, title, link, author, date, picture and the article's HTML), as the
// sites write them: CDATA or escaped HTML, HTML entities in titles, dates in
// RFC 822 or RFC 3339, relative links, other charsets (MuPDF's parser turns
// UTF-16 and the usual 8-bit ones into UTF-8). Also a site's page that links
// to its feed, an OPML list of feeds, and the feed kept on the card (its
// articles merged with the ones kept before, newest first). Pure: parses and
// writes text; newsjob.c asks the sites. Tested on the PC.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FEED_URL_MAX 1024
#define FEED_TEXT_MAX 256
#define FEED_ID_MAX 1024
#define FEED_HTML_MAX (512 * 1024)  // An article's HTML, at most (the rest is cut).
#define FEED_KEEP 100               // Articles kept per feed, the newest.
#define FEED_HASH_LEN 16            // A feed's or an article's name on the card: 16 hex digits.

typedef struct {
    char id[FEED_ID_MAX];          // Its guid or id; its link if it has none.
    char title[FEED_TEXT_MAX];     // Plain text.
    char link[FEED_URL_MAX];       // The article on the site, absolute; "" if none.
    char author[FEED_TEXT_MAX];
    char image[FEED_URL_MAX];      // A picture for the list (an enclosure, media:, or its first <img>); "" none.
    int image_score;               // How good it looked to the parser (the largest offered wins).
    int64_t date;                  // Seconds since 1970 (UTC); 0 if it has none (or it can't be read).
    char *html;                    // The article (content:encoded, content, else description/summary); malloc'd, may be NULL.
} FeedItem;

typedef struct {
    char title[FEED_TEXT_MAX];
    char link[FEED_URL_MAX];  // The site.
    FeedItem *items;
    int count;
} Feed;

// Parses a feed fetched from base_url (for relative links). False if it isn't one
// (a web page, or not even XML), with out empty.
bool feedsParse(const char *data, size_t size, const char *base_url, Feed *out);
void feedsFree(Feed *f);

// An RFC 822 ("Tue, 10 Jun 2003 04:00:00 +0200") or RFC 3339 ("2003-06-10T04:00:00.5Z")
// date, as seconds since 1970 (UTC). 0 if it can't be read.
int64_t feedsParseDate(const char *text);
// A date as the list shows it: "2026-11-27". "" for 0.
void feedsDateText(int64_t date, char *out, size_t size);
// And with its time (UTC), as feedsParseDate reads it back: "2026-11-27 14:05". "" for 0.
void feedsDateTimeText(int64_t date, char *out, size_t size);

// Text as it's shown: tags taken out, HTML entities (&amp;, &eacute;, &#233;) made
// characters, whitespace runs made one space, cut to fit (whole characters).
void feedsPlainText(const char *in, char *out, size_t size);

// A site's page (HTML) that links to its feed (<link rel="alternate" type="application/rss+xml">):
// the feed's address, absolute, into out. False if it links to none.
bool feedsDiscover(const char *html, size_t size, const char *base_url, char *out, size_t out_size);

// The articles of fresh (just fetched) taken into kept (as read from the card): one of each id
// (fresh's is newer), newest first (those without a date after, in the feed's order), at most
// max. fresh is left empty. False if memory ran out (kept is as it was).
bool feedsMerge(Feed *kept, Feed *fresh, int max);

// The feed as KaiReader keeps it on the card: an Atom feed feedsParse reads back the same.
// malloc'd, NUL-terminated (its size in *size); NULL if memory ran out.
char *feedsWrite(const Feed *f, size_t *size);

// One feed of an OPML list.
typedef struct {
    char url[FEED_URL_MAX];
    char title[FEED_TEXT_MAX];
} FeedOutline;

// The feeds of an OPML file (outlines with an xmlUrl, in folders too). malloc'd into *out
// (the caller frees it); how many, -1 if it isn't OPML.
int feedsParseOpml(const char *data, size_t size, FeedOutline **out);

// The name of a text (a feed's address, an article's id) on the card: FEED_HASH_LEN hex digits.
void feedsHash(const char *text, char out[FEED_HASH_LEN + 1]);
