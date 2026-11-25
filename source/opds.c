#include "opds.h"

#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define BOOKS_GROWTH 32
#define FEEDS_GROWTH 16

// The kinds read, best first (an entry with an EPUB and a PDF gives its EPUB).
static const struct {
    const char *mime;
    const char *ext;
} KINDS[] = {
    {"application/epub+zip", ".epub"},
    {"application/x-fictionbook+xml", ".fb2"},
    {"application/fb2+zip", ".fb2"},
    {"application/x-mobipocket-ebook", ".mobi"},
    {"application/pdf", ".pdf"},
    {"application/vnd.comicbook+zip", ".cbz"},
    {"application/x-cbz", ".cbz"},
    {"application/zip", ".cbz"},  // Komga's CBZ.
    {"application/vnd.comicbook-rar", ".cbr"},
    {"application/x-cbr", ".cbr"},
    {"application/x-rar-compressed", ".cbr"},
    {"application/vnd.rar", ".cbr"},
    {"application/x-cb7", ".cb7"},
    {"application/x-7z-compressed", ".cb7"},
    {"application/x-cbt", ".cbt"},
    {"application/vnd.ms-xpsdocument", ".xps"},
    {"text/plain", ".txt"},
};
#define KIND_COUNT ((int)(sizeof(KINDS) / sizeof(KINDS[0])))

static const char *const EXTS[] = {".epub", ".fb2", ".mobi", ".pdf", ".cbz", ".cbr", ".cb7", ".cbt", ".xps", ".txt"};

// Its rank among KINDS (lower is better), KIND_COUNT if it isn't read.
static int rankOf(const char *mime, const char *href)
{
    if (mime) {
        size_t len = strcspn(mime, "; ");  // "application/epub+zip; charset=..." too.
        for (int i = 0; i < KIND_COUNT; i++)
            if (strlen(KINDS[i].mime) == len && !strncasecmp(mime, KINDS[i].mime, len))
                return i;
    }
    if (href) {  // A link that says only its name ("/download/12/cbz", "book.cbr").
        for (int i = 0; i < KIND_COUNT; i++) {
            size_t n = strlen(KINDS[i].ext), h = strcspn(href, "?#");
            if (h >= n && !strncasecmp(href + h - n, KINDS[i].ext, n))
                return i;
            if (h >= n && href[h - n] == '/' && !strncasecmp(href + h - n + 1, KINDS[i].ext + 1, n - 1))
                return i;
        }
    }
    return KIND_COUNT;
}

const char *opdsExtensionFor(const char *mime, const char *href)
{
    int r = rankOf(mime, href);
    if (r == KIND_COUNT)
        return NULL;
    for (size_t i = 0; i < sizeof(EXTS) / sizeof(EXTS[0]); i++)
        if (!strcmp(EXTS[i], KINDS[r].ext))
            return EXTS[i];
    return NULL;
}

bool opdsResolve(const char *base, const char *href, char *out, size_t size)
{
    if (!href || !href[0])
        return false;
    int n;
    const char *scheme = strstr(href, "://");
    if (scheme && (size_t)(scheme - href) < 10) {  // Absolute.
        n = snprintf(out, size, "%s", href);
    } else {
        const char *host = strstr(base, "://");
        if (!host)
            return false;
        const char *path = strchr(host + 3, '/');
        size_t root = path ? (size_t)(path - base) : strlen(base);
        if (href[0] == '/' && href[1] == '/') {  // Scheme-relative.
            n = snprintf(out, size, "%.*s%s", (int)(host - base + 1), base, href);
        } else if (href[0] == '/') {  // From the host's root.
            n = snprintf(out, size, "%.*s%s", (int)root, base, href);
        } else {  // Relative to the base's folder (its query cut off).
            size_t q = strcspn(base, "?#");
            size_t dir = q;
            while (dir > root && base[dir - 1] != '/')
                dir--;
            if (dir <= root)
                n = snprintf(out, size, "%.*s/%s", (int)root, base, href);
            else
                n = snprintf(out, size, "%.*s%s", (int)dir, base, href);
        }
    }
    return n > 0 && (size_t)n < size;
}

static void quiet(void *user, const char *message)
{
    (void)user;
    (void)message;
}

static bool tagIs(fz_xml *node, const char *name)
{
    const char *tag = fz_xml_tag(node);
    if (!tag)
        return false;
    const char *colon = strrchr(tag, ':');
    return !strcmp(colon ? colon + 1 : tag, name);
}

// The first child element called name.
static fz_xml *child(fz_xml *node, const char *name)
{
    for (fz_xml *c = node ? fz_xml_down(node) : NULL; c; c = fz_xml_next(c))
        if (tagIs(c, name))
            return c;
    return NULL;
}

// Its text, whitespace runs made one space, into out.
static void textInto(fz_xml *node, char *out, size_t size)
{
    if (size)
        out[0] = '\0';
    size_t o = 0;
    bool space = false;
    for (fz_xml *c = node ? fz_xml_down(node) : NULL; c; c = fz_xml_next(c)) {
        const char *t = fz_xml_text(c);
        for (const unsigned char *s = (const unsigned char *)(t ? t : ""); *s && o + 1 < size; s++) {
            if (*s <= ' ' || *s == 0x7F) {
                space = o > 0;
                continue;
            }
            if (space && o + 2 < size)
                out[o++] = ' ';
            space = false;
            out[o++] = (char)*s;
        }
    }
    while (o > 0 && ((unsigned char)out[o - 1] & 0xC0) == 0x80)  // Not inside a UTF-8 sequence cut short.
        o--;
    if (o > 0 && (unsigned char)out[o - 1] >= 0xC0)
        o--;
    if (size)
        out[o] = '\0';
}

static bool hasWord(const char *text, const char *word)
{
    return text && strstr(text, word) != NULL;
}

static bool addFeed(OpdsFeed *f, const char *url, fz_xml *entry)
{
    for (int i = 0; i < f->feed_count; i++)
        if (!strcmp(f->feeds[i].url, url))
            return true;
    if (f->feed_count % FEEDS_GROWTH == 0) {
        void *grown = realloc(f->feeds, (size_t)(f->feed_count + FEEDS_GROWTH) * sizeof(*f->feeds));
        if (!grown)
            return false;
        f->feeds = grown;
    }
    OpdsLink *l = &f->feeds[f->feed_count++];
    snprintf(l->url, sizeof(l->url), "%s", url);
    textInto(child(entry, "title"), l->title, sizeof(l->title));
    textInto(child(entry, "content"), l->summary, sizeof(l->summary));
    return true;
}

// One entry: a book (an acquisition link of a kind read), or a way to another feed.
static void readEntry(OpdsFeed *f, fz_xml *entry, const char *base)
{
    OpdsBook b;
    memset(&b, 0, sizeof(b));
    b.size = -1;
    int best = KIND_COUNT;
    char nav[OPDS_URL_MAX] = "";
    for (fz_xml *l = fz_xml_down(entry); l; l = fz_xml_next(l)) {
        if (tagIs(l, "thumbnail") && !b.cover[0]) {  // media:thumbnail url="...".
            const char *u = fz_xml_att(l, "url");
            if (u)
                opdsResolve(base, u, b.cover, sizeof(b.cover));
            continue;
        }
        if (!tagIs(l, "link"))
            continue;
        const char *rel = fz_xml_att(l, "rel"), *type = fz_xml_att(l, "type"), *href = fz_xml_att(l, "href");
        if (!href)
            continue;
        char url[OPDS_URL_MAX];
        if (!opdsResolve(base, href, url, sizeof(url)))
            continue;
        bool enclosure = rel && !strcmp(rel, "enclosure");  // An Atom feed's download (Standard Ebooks').
        if (hasWord(rel, "opds-spec.org/acquisition") || enclosure) {
            if (hasWord(rel, "borrow") || hasWord(rel, "buy") || hasWord(rel, "subscribe") || hasWord(rel, "sample"))
                continue;  // Not a book to read here.
            int r = rankOf(type, enclosure ? NULL : href);  // An enclosure's kind is its type alone.
            if (r < best) {
                best = r;
                snprintf(b.acquire, sizeof(b.acquire), "%s", url);
                const char *length = fz_xml_att(l, "length");
                b.size = length ? strtoll(length, NULL, 10) : -1;
            }
        } else if (hasWord(rel, "opds-spec.org/image/thumbnail")) {
            snprintf(b.cover, sizeof(b.cover), "%s", url);  // The small one: it comes quicker.
        } else if (hasWord(rel, "opds-spec.org/image") && !b.cover[0]) {
            snprintf(b.cover, sizeof(b.cover), "%s", url);
        } else if (type && hasWord(type, "application/atom+xml") && !hasWord(rel, "alternate") && !nav[0]) {
            snprintf(nav, sizeof(nav), "%s", url);  // A subsection (or "kind=acquisition" feed).
        }
    }
    if (best < KIND_COUNT) {
        textInto(child(entry, "id"), b.id, sizeof(b.id));
        textInto(child(entry, "title"), b.title, sizeof(b.title));
        textInto(child(child(entry, "author"), "name"), b.author, sizeof(b.author));
        fz_xml *cat = child(entry, "category");
        const char *genre = cat ? fz_xml_att(cat, "label") : NULL;
        if (!genre && cat)
            genre = fz_xml_att(cat, "term");
        snprintf(b.genre, sizeof(b.genre), "%s", genre ? genre : "");
        snprintf(b.ext, sizeof(b.ext), "%s", KINDS[best].ext);
        if (!b.id[0])
            snprintf(b.id, sizeof(b.id), "%s", b.acquire);
        if (f->book_count % BOOKS_GROWTH == 0) {
            void *grown = realloc(f->books, (size_t)(f->book_count + BOOKS_GROWTH) * sizeof(*f->books));
            if (!grown)
                return;
            f->books = grown;
        }
        f->books[f->book_count++] = b;
    } else if (nav[0]) {
        addFeed(f, nav, entry);
    }
}

bool opdsParse(const char *xml, size_t size, const char *base_url, OpdsFeed *out)
{
    memset(out, 0, sizeof(*out));
    fz_context *ctx = fz_new_context(NULL, NULL, 8u << 20);
    if (!ctx)
        return false;
    fz_set_warning_callback(ctx, quiet, NULL);  // A page that isn't a feed is said by returning false.
    fz_set_error_callback(ctx, quiet, NULL);
    bool ok = false;
    fz_buffer *buf = NULL;
    fz_xml *doc = NULL;
    fz_var(buf);
    fz_var(doc);
    fz_var(ok);
    fz_try(ctx)
    {
        buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)xml, size);
        doc = fz_parse_xml(ctx, buf, 0);
        fz_xml *feed = fz_xml_root(doc);
        while (feed && !tagIs(feed, "feed"))
            feed = fz_xml_next(feed);
        if (feed) {
            ok = true;
            textInto(child(feed, "title"), out->title, sizeof(out->title));
            for (fz_xml *n = fz_xml_down(feed); n; n = fz_xml_next(n)) {
                if (tagIs(n, "entry")) {
                    readEntry(out, n, base_url);
                } else if (tagIs(n, "link")) {
                    const char *rel = fz_xml_att(n, "rel"), *href = fz_xml_att(n, "href");
                    if (rel && href && !strcmp(rel, "next"))
                        opdsResolve(base_url, href, out->next, sizeof(out->next));
                }
            }
        }
    }
    fz_always(ctx)
    {
        fz_drop_xml(ctx, doc);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx)
    {
        ok = false;
    }
    fz_drop_context(ctx);
    if (!ok)
        opdsFeedFree(out);
    return ok;
}

void opdsFeedFree(OpdsFeed *f)
{
    free(f->books);
    free(f->feeds);
    memset(f, 0, sizeof(*f));
}
