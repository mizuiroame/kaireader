#include "catalogs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lang.h"
#include "netlist.h"

#define GUTENBERG "https://www.gutenberg.org"
#define BROWSE_COUNT CATALOG_SEARCH  // The lists that browse a catalog; the search is the last.

static const char *const NAMES[CATALOG_COUNT] = {
    "Project Gutenberg: most popular",
    "Standard Ebooks: newest",
    "textos.info: most popular (Spanish)",
    "Ebooks libres et gratuits: newest (French)",
    "unglue.it: most popular (Creative Commons)",
    "Project Gutenberg: search",
};

static const char *const SHOW_LINES[CATALOG_COUNT] = {
    N_("Show Project Gutenberg's most popular"),
    N_("Show Standard Ebooks' newest"),
    N_("Show textos.info's most popular, in Spanish"),
    N_("Show Ebooks libres et gratuits' newest, in French"),
    N_("Show unglue.it's most popular, under Creative Commons"),
    N_("Show your last search"),
};

// Each catalog's search, by the list that browses it: its first page (%s: the words), the
// search list's name, the keyboard's title, the + menu's line, and what to try when nothing's found.
static const struct {
    const char *url, *name, *prompt, *line, *hint;
} SEARCHES[BROWSE_COUNT] = {
    {GUTENBERG "/ebooks/search.opds/?query=%s", "Project Gutenberg: search",
     N_("Search Project Gutenberg: a title, an author, a subject"), N_("Search Project Gutenberg"),
     N_("Try other words: a title, an author or a subject, in English.")},
    {"https://standardebooks.org/feeds/atom/all?query=%s", "Standard Ebooks: search",
     N_("Search Standard Ebooks: a title, an author, a subject"), N_("Search Standard Ebooks"),
     N_("Try other words: a title, an author or a subject, in English.")},
    {"https://www.textos.info/busqueda.atom&query=%s", "textos.info: search",
     N_("Search textos.info: a title or an author, in Spanish"), N_("Search textos.info"),
     N_("Try other words: a title or an author, in Spanish.")},
    {"https://www.ebooksgratuits.com/opds/feed.php?mode=search&query=%s", "Ebooks libres et gratuits: search",
     N_("Search Ebooks libres et gratuits: words of a title, in French"), N_("Search Ebooks libres et gratuits"),
     N_("Try other words from a title, in French.")},
    {"https://unglue.it/api/opds/search/?q=%s", "unglue.it: search",
     N_("Search unglue.it: a title, an author, a subject"), N_("Search unglue.it"),
     N_("Try other words: a title, an author or a subject, in English.")},
};

const char *catalogName(CatalogList list)
{
    return list >= 0 && list < CATALOG_COUNT ? NAMES[list] : NAMES[0];
}

const char *catalogShowLine(CatalogList list)
{
    return list >= 0 && list < CATALOG_COUNT ? SHOW_LINES[list] : SHOW_LINES[0];
}

CatalogList catalogSearchOf(CatalogList shown, CatalogList last)
{
    if (shown >= 0 && shown < BROWSE_COUNT)
        return shown;
    return last >= 0 && last < BROWSE_COUNT ? last : CATALOG_POPULAR;
}

static int searchIndex(CatalogList in)
{
    return in >= 0 && in < BROWSE_COUNT ? (int)in : 0;
}

const char *catalogSearchName(CatalogList in)
{
    return SEARCHES[searchIndex(in)].name;
}

const char *catalogSearchPrompt(CatalogList in)
{
    return SEARCHES[searchIndex(in)].prompt;
}

const char *catalogSearchLine(CatalogList in)
{
    return SEARCHES[searchIndex(in)].line;
}

const char *catalogSearchHint(CatalogList in)
{
    return SEARCHES[searchIndex(in)].hint;
}

// query as a URL's query value: letters, digits and -._~ as they are, spaces as +, the rest %XX.
static bool encode(const char *query, char *out, size_t size)
{
    size_t o = 0;
    for (const unsigned char *s = (const unsigned char *)query; *s; s++) {
        if (o + 4 > size)
            return false;
        if (isalnum(*s) || strchr("-._~", *s))
            out[o++] = (char)*s;
        else if (*s == ' ')
            out[o++] = '+';
        else
            o += (size_t)snprintf(out + o, size - o, "%%%02X", *s);
    }
    out[o] = '\0';
    return true;
}

bool catalogUrl(CatalogList list, CatalogList search_in, const char *query, char *out, size_t size)
{
    static const char *const FIRST_PAGES[BROWSE_COUNT] = {
        GUTENBERG "/ebooks/search.opds/?sort_order=downloads",
        "https://standardebooks.org/feeds/atom/new-releases",
        "https://www.textos.info/populares.atom",
        "https://www.ebooksgratuits.com/opds/feed.php",
        "https://unglue.it/api/opds/epub/?order_by=popular",  // Its books with an EPUB.
    };
    int n;
    if (list >= 0 && list < BROWSE_COUNT) {
        n = snprintf(out, size, "%s", FIRST_PAGES[list]);
    } else if (list == CATALOG_SEARCH) {
        char words[CATALOG_QUERY_MAX * 3 + 1];
        while (query && *query == ' ')
            query++;
        if (!query || !*query || !encode(query, words, sizeof(words)))
            return false;
        n = snprintf(out, size, SEARCHES[searchIndex(search_in)].url, words);
    } else {
        return false;
    }
    return n > 0 && (size_t)n < size;
}

bool catalogNextPage(const char *asked, const char *next, char *out, size_t size)
{
    int n = snprintf(out, size, "%s", next);
    // unglue.it's search leaves its words out of its next page: put back, or it'd go on with every book.
    static const char SEARCH[] = "unglue.it/api/opds/search/";
    const char *q = strstr(asked, "?q=");
    if (!q)
        q = strstr(asked, "&q=");
    if (next[0] && strstr(next, SEARCH) && strstr(asked, SEARCH) && q && !strstr(next, "?q=") && !strstr(next, "&q=")) {
        size_t words = strcspn(q + 1, "&#");
        n = snprintf(out, size, "%s%s%.*s", next, strchr(next, '?') ? "&" : "?", (int)words, q + 1);
    }
    return n >= 0 && (size_t)n < size;
}

// Words that follow a surname after a comma without being a first name ("King, Jr.").
static bool isSuffix(const char *word)
{
    static const char *const SUFFIXES[] = {"Jr", "Jr.", "Sr", "Sr.", "II", "III", "IV"};
    for (size_t i = 0; i < sizeof(SUFFIXES) / sizeof(SUFFIXES[0]); i++)
        if (!strcmp(word, SUFFIXES[i]))
            return true;
    return false;
}

bool catalogAuthorAsWritten(const char *author, char *out, size_t size)
{
    const char *comma = strchr(author, ',');
    if (!comma || strchr(comma + 1, ','))
        return false;  // None, or several (a list of authors, years): left as it is.
    const char *given = comma + 1;
    while (*given == ' ')
        given++;
    size_t surname = (size_t)(comma - author);
    while (surname > 0 && author[surname - 1] == ' ')
        surname--;
    if (!surname || !*given || isSuffix(given))
        return false;
    for (const char *s = given; *s; s++)
        if (isdigit((unsigned char)*s))
            return false;  // "1802-1885": not a name.
    int n = snprintf(out, size, "%s %.*s", given, (int)surname, author);
    return n > 0 && (size_t)n < size;
}

// Ebooks libres et gratuits gives its authors as "Surname, Name": shown as written, kept for sorting.
static void writeAuthor(OpdsBook *b)
{
    if (!strstr(b->acquire, "://www.ebooksgratuits.com/"))
        return;
    char written[OPDS_TEXT_MAX];
    if (!catalogAuthorAsWritten(b->author, written, sizeof(written)))
        return;
    snprintf(b->author_sort, sizeof(b->author_sort), "%s", b->author);
    snprintf(b->author, sizeof(b->author), "%s", written);
}

// Gutenberg's book number in a link to its page ("https://www.gutenberg.org/ebooks/84.opds"); -1 if it isn't one.
static long gutenbergNumber(const char *url)
{
    const char *at = strstr(url, "gutenberg.org/ebooks/");
    if (!at)
        return -1;
    at += strlen("gutenberg.org/ebooks/");
    char *end;
    long n = strtol(at, &end, 10);
    return end != at && n > 0 && !strcmp(end, ".opds") ? n : -1;
}

int catalogBooks(const OpdsFeed *page, OpdsBook **out)
{
    *out = calloc((size_t)(page->book_count + page->feed_count + 1), sizeof(**out));
    if (!*out)
        return 0;
    int n = 0;
    for (int i = 0; i < page->book_count; i++) {
        (*out)[n] = page->books[i];
        writeAuthor(&(*out)[n++]);
    }
    for (int i = 0; i < page->feed_count; i++) {
        const OpdsLink *l = &page->feeds[i];
        long number = gutenbergNumber(l->url);
        if (number < 0 || !l->title[0])
            continue;  // A list, not a book ("Sort by title", an author's page...).
        OpdsBook *b = &(*out)[n++];
        memset(b, 0, sizeof(*b));
        snprintf(b->id, sizeof(b->id), "pg:%ld", number);
        snprintf(b->title, sizeof(b->title), "%s", l->title);
        snprintf(b->author, sizeof(b->author), "%s", l->summary);
        snprintf(b->acquire, sizeof(b->acquire), GUTENBERG "/ebooks/%ld.epub3.images", number);
        snprintf(b->ext, sizeof(b->ext), "%s", ".epub");
        snprintf(b->cover, sizeof(b->cover), GUTENBERG "/cache/epub/%ld/pg%ld.cover.medium.jpg", number, number);
        b->size = -1;
    }
    return n;
}

void catalogFileName(const OpdsBook *b, char *out, size_t size)
{
    char name[OPDS_TEXT_MAX * 2 + OPDS_EXT_MAX + 8];
    snprintf(name, sizeof(name), "%s%s%s%s", b->title[0] ? b->title : "Book", b->author[0] ? " - " : "", b->author,
             b->ext[0] ? b->ext : ".epub");
    netPlainName(name, false, out, size);
}
